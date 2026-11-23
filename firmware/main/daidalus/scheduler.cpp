#include "scheduler.hpp"

#include <new>

#include "adsbee/adsbee_link.hpp"
#include "baro/barometer.hpp"
#include "display/active_display_settings.hpp"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "loop.hpp"
#include "owner.hpp"
#include "sdkconfig.h"
#include "settings/settings.hpp"
#include "traffic/traffic.hpp"

namespace conflicts {
namespace {
static_assert(
	CONFIG_AIRGREP_LOOP_STACK >= 32768,
	"the loop needs 32 KB of stack, delete the sdkconfig of the build");
constexpr uint32_t kPeriodMs = 1000;

// All of these belong to the loop task. The engine works on them outside the
// lock of the State, which then takes a copy.
gsl::owner<Monitor*> monitor;
traffic::Tracked* listed;
StateData work;
adsbee::StateData link;
barometer::StateData pressure;

void publish() {
	work.minimum_free_stack_bytes =
		uxTaskGetStackHighWaterMark(nullptr) * sizeof(StackType_t);
	State::update([](State& state) { static_cast<StateData&>(state) = work; });
}

loop::Result band(size_t guide) {
	const int64_t began = esp_timer_get_time();
	if (monitor->calculate_bands(guide, work)) {
		work.bands[guide].calculation_us =
			static_cast<uint32_t>(esp_timer_get_time() - began);
	}
	publish();
	return loop::Result::done;
}

loop::Result alert(loop::Handle, loop::Value&) {
	const int64_t began = esp_timer_get_time();
	link = adsbee::State::copy();
	pressure = barometer::State::copy();
	const adsbee::Gnss& gnss = link.gnss_at(began);
	traffic::Reference own{gnss.latitude, gnss.longitude};
	uint16_t present = 0;
	const size_t count =
		traffic::nearest(gnss.fix != adsbee::Fix::none ? &own : nullptr, listed,
						 traffic::kListed, present)
			.value_or(0);
	const int64_t read = esp_timer_get_time();
	const bool on = display_settings::snapshot().conflict_indicator;
	const conflicts::ConflictInput sources{link, std::span(listed, count),
										   pressure, began,
										   settings::copy().own_address};
	auto follow = monitor->alert(sources, on, work);
	work.alert_finished_us = esp_timer_get_time();
	work.alert_duration_us =
		static_cast<uint32_t>(work.alert_finished_us - began);
	work.input_read_us = static_cast<uint32_t>(read - began);
	publish();
	if (follow == BandSchedule::skip) {
		return loop::Result::done;
	}
	// Yield between guides so traffic updates can run.
	for (size_t guide = 0; guide < kGuided; guide++) {
		auto task = [guide](loop::Handle, loop::Value&) { return band(guide); };
		if (!loop::submit(kGuides[guide].task, task).valid()) {
			monitor->release_pending_band_task();
		}
	}
	return loop::Result::done;
}
}

esp_err_t start() {
	listed = static_cast<traffic::Tracked*>(
		heap_caps_malloc(sizeof(traffic::Tracked) * traffic::kListed,
						 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
	monitor = new (std::nothrow) Monitor;
	if (!listed || !monitor ||
		!loop::schedule_periodic(kPeriodMs, {"conflict alerts", alert})
			 .valid()) {
		return ESP_ERR_NO_MEM;
	}
	return ESP_OK;
}
}

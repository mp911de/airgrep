#include "render_benchmark.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>

#include "display/display.hpp"
#ifdef CONFIG_AIRGREP_PROFILER
#include "render_profile.hpp"
#endif

namespace hwtest::render {
namespace {
using Clock = std::chrono::steady_clock;
using Microseconds = std::chrono::microseconds;
constexpr size_t kFrames = 20;
enum class Scene { quiet, traffic, recovery };

void update(Scene scene, unsigned step, bool history = true) {
	display::ViewState model;
	model.hour = 12;
	model.minute = static_cast<uint8_t>(step % 60);
	model.fix = display::Fix::fix_3d;
	model.satellites = 12;
	model.battery = display::Battery::discharging;
	model.battery_percent = 80;
	model.wifi_network = "";
	model.wifi_password = "";
	std::array<display::Target, 12> targets{};
	if (scene != Scene::quiet) {
		for (unsigned i = 0; i < targets.size(); ++i) {
			auto& target = targets[i];
			target.address = 0xa00001 + i;
			(void)std::snprintf(target.callsign, sizeof target.callsign,
								"TEST%02u", i);
			target.kind = display::Kind::light;
			target.radar_position = display::RadarPosition::inside_ring;
			target.bearing_deg = static_cast<uint16_t>((30 * i + step) % 360);
			target.track_deg = (target.bearing_deg + 90) % 360;
			target.track_valid = true;
			target.distance_nm = 1.0f + 0.2f * static_cast<float>(i);
			target.relative_altitude_ft = static_cast<int16_t>(100 * i);
			const float angle = static_cast<float>(target.bearing_deg) *
								std::numbers::pi_v<float> / 180;
			const float right = std::sin(angle), ahead = std::cos(angle);
			target.tail_count = display::kTailSamples;
			for (size_t j = 0; j < display::kTailSamples; ++j) {
				const float behind = 0.02f * static_cast<float>(j + 1);
				target.tail[j] = {target.distance_nm * right - behind * ahead,
								  target.distance_nm * ahead + behind * right};
			}
		}
		model.traffic = targets;
		model.traffic_inside = targets.size();
		model.track_history = history;
	}
	if (scene == Scene::recovery) {
		static constexpr conflicts::Band bands[] = {
			{0, 360, conflicts::BandRegion::recovery}};
		model.conflict_indicator = true;
		model.track_bands = bands;
	}
	display::update(model);
}

// A refresh timer may run with no invalidated area. Only a completed draw
// counts.
struct Measurement {
	Clock::time_point start{};
	Microseconds last{};
	unsigned frames = 0;
#ifdef CONFIG_AIRGREP_PROFILER
	DrawProfile* profile{};
#endif

	static void event(lv_event_t* event) {
		auto& sample =
			*static_cast<Measurement*>(lv_event_get_user_data(event));
		if (lv_event_get_code(event) == LV_EVENT_RENDER_START) {
			sample.start = Clock::now();
#ifdef CONFIG_AIRGREP_PROFILER
			if (sample.profile) {
				sample.profile->start();
			}
#endif
		} else {
#ifdef CONFIG_AIRGREP_PROFILER
			if (sample.profile) {
				DrawProfile::stop();
			}
#endif
			sample.last = std::chrono::duration_cast<Microseconds>(
				Clock::now() - sample.start);
			++sample.frames;
		}
	}
};

class Run {
   public:
	Run(lv_display_t* panel, Measurement& sample)
		: panel_(panel), sample_(sample) {
		lv_display_add_event_cb(panel, Measurement::event,
								LV_EVENT_RENDER_START, &sample);
		lv_display_add_event_cb(panel, Measurement::event,
								LV_EVENT_RENDER_READY, &sample);
	}
	~Run() {
		lv_timer_pause(lv_display_get_refr_timer(panel_));
		lv_display_remove_event_cb_with_user_data(panel_, Measurement::event,
												  &sample_);
	}
	Run(const Run&) = delete;
	Run& operator=(const Run&) = delete;
	Run(Run&&) = delete;
	Run& operator=(Run&&) = delete;

   private:
	lv_display_t* panel_;
	Measurement& sample_;
};

void full_frame(lv_display_t* panel) {
	lv_obj_invalidate(lv_display_get_screen_active(panel));
	lv_timer_t* timer = lv_display_get_refr_timer(panel);
	lv_timer_pause(timer);
	lv_display_refr_timer(timer);
}

bool measure(Output& output, lv_display_t* panel, Measurement& sample,
			 Scene scene, const char* name, bool full,
			 void (*wait_one_tick)()) {
	std::array<Microseconds, kFrames> times{};
	Clock::time_point began{};
	for (unsigned step = 0; step <= kFrames; ++step) {
		update(scene, step);
		if (full || step == 0) {
			lv_obj_invalidate(lv_display_get_screen_active(panel));
		}
		const unsigned before = sample.frames;
		const auto deadline = Clock::now() + std::chrono::seconds(5);
		do {
			wait_one_tick();
			lv_timer_handler();
		} while (sample.frames == before && Clock::now() < deadline);
		if (sample.frames != before + 1) {
			output.print("%s %s step %u completed %u renders\n", name,
						 full ? "full" : "dirty", step, sample.frames - before);
			return false;
		}
		if (step == 0) {
			began = Clock::now();
		} else {
			times[step - 1] = sample.last;
		}
	}
	const std::chrono::duration<double> elapsed = Clock::now() - began;
	std::ranges::sort(times);
	output.print(
		"%s %s  %u renders  %.2f renders/s  draw p95 %.2f ms\n", name,
		full ? "full" : "dirty", (unsigned)kFrames, kFrames / elapsed.count(),
		static_cast<double>(times[(95 * kFrames + 99) / 100 - 1].count()) /
			1000.0);
	return true;
}
}

void periodic(Output& output, lv_display_t* panel, void (*wait_one_tick)()) {
	Measurement sample;
	const Run run(panel, sample);
	lv_timer_resume(lv_display_get_refr_timer(panel));
	for (const auto& [scene, name] : {std::pair{Scene::quiet, "quiet"},
									  {Scene::traffic, "traffic"},
									  {Scene::recovery, "recovery"}}) {
		for (const bool full : {false, true}) {
			if (!measure(output, panel, sample, scene, name, full,
						 wait_one_tick)) {
				output.report("render_periodic", false,
							  "unexpected render count");
				return;
			}
		}
	}
	output.report("render_periodic", true, "120 measured renders");
}

void single(Output& output, lv_display_t* panel) {
	Measurement sample;
	const Run run(panel, sample);
	update(Scene::traffic, 0);
	full_frame(panel);
	output.print("traffic full  %u renders  draw %.2f ms\n", sample.frames,
				 static_cast<double>(sample.last.count()) / 1000.0);
	output.report("render_single", sample.frames == 1, "manual refresh");
}

#ifdef CONFIG_AIRGREP_PROFILER
void profile(Output& output, lv_display_t* panel, void (*wait_one_tick)()) {
	static_assert(LV_USE_OS == LV_OS_NONE,
				  "Draw hooks and refresh must run on the same task");
	using Milliseconds = std::chrono::duration<double, std::milli>;
	Measurement sample;
	const Run run(panel, sample);
	struct Case {
		const char* name;
		Scene scene;
		bool history;
	};
	for (const auto& test :
		 {Case{"traffic", Scene::traffic, true},
		  Case{"traffic without history", Scene::traffic, false},
		  Case{"recovery", Scene::recovery, true}}) {
		wait_one_tick();
		update(test.scene, 0, test.history);
		const auto before = sample.frames;
		full_frame(panel);
		full_frame(panel);
		const auto baseline = sample.last;
		DrawProfile profile;
		sample.profile = &profile;
		full_frame(panel);
		sample.profile = nullptr;
		output.print(
			"%s full  baseline %.2f ms  profiled %.2f ms  delta %+.2f ms\n",
			test.name, Milliseconds(baseline).count(),
			Milliseconds(sample.last).count(),
			Milliseconds(sample.last - baseline).count());
		if (sample.frames != before + 3 ||
			!profile.print(output, sample.last)) {
			output.report("render_profile", false,
						  "incomplete frame or draw profile");
			return;
		}
	}
	output.report("render_profile", true,
				  "three matched full-frame comparisons");
}
#endif
}

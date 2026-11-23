#include "hwtest.hpp"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>

#include "baro/barometer.hpp"
#include "battery/max17048.hpp"
#include "charger/bq25798.hpp"
#include "device.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "i2c_expander/tca9534.hpp"
#include "soc/extmem_reg.h"
#include "soc/soc.h"
#include "usb/fusb303.hpp"

#ifdef HWTEST_BENCHMARKS
#include "benchmark.hpp"
#include "conflicts/conflicts.hpp"
#endif

namespace hwtest {
void Output::print(const char* format, ...) {
	va_list arguments;
	va_start(arguments, format);
	(void)std::vfprintf(stream_, format, arguments);
	va_end(arguments);
	(void)std::fflush(stream_);
}

void Output::report(const char* name, bool pass, const char* detail) {
	print("HWTEST %s %s %s\n", name, pass ? "PASS" : "FAIL", detail);
}

namespace {
constexpr int64_t kReadyTimeoutUs = 6000000;
constexpr uint32_t kStackBytes = 32 * 1024;

void charger_identity(Output& output) {
	const auto charger = charger::State::copy();
	char detail[32];
	(void)std::snprintf(detail, sizeof detail, "REG48 0x%02X",
						charger.readings.raw[0x48]);
	output.report("charger_identity", charger.identity_known, detail);
}

void battery_identity(Output& output) {
	const auto battery = battery::State::copy();
	char detail[32];
	(void)std::snprintf(detail, sizeof detail, "VERSION 0x%04X",
						battery.raw_version);
	output.report("battery_identity", battery.identity_known, detail);
}

void barometer_identity(Output& output) {
	const auto baro = barometer::State::copy();
	char detail[32];
	(void)std::snprintf(detail, sizeof detail, "CHIP_ID 0x%02X", baro.chip_id);
	output.report("barometer_identity", baro.identity_known, detail);
}

void fusb_identity(Output& output) {
	const auto fusb = fusb::State::copy();
	char detail[32];
	(void)std::snprintf(detail, sizeof detail, "DEVICE_ID 0x%02X",
						fusb.raw_identity);
	output.report("fusb_identity", fusb.identity_known, detail);
}

void expander_identity(Output& output) {
	const auto expander = expander::State::copy();
	char detail[32];
	(void)std::snprintf(detail, sizeof detail, "INPUT_PORT 0x%02X",
						expander.raw_input);
	output.report("expander_identity", expander.identity_known, detail);
}

template <typename State>
bool ready() {
	return is_ready(State::copy());
}

void report_driver_readiness(Output& output, int64_t deadline_us) {
	struct {
		const char* name{};
		bool (*ready)(){};
		int64_t at_us = 0;
	} drivers[] = {{"charger_ready", ready<charger::State>},
				   {"battery_ready", ready<battery::State>},
				   {"barometer_ready", ready<barometer::State>},
				   {"fusb_ready", ready<fusb::State>},
				   {"expander_ready", ready<expander::State>}};
	while (esp_timer_get_time() < deadline_us) {
		bool waiting = false;
		for (auto& driver : drivers) {
			if (!driver.at_us && driver.ready()) {
				driver.at_us = esp_timer_get_time();
			}
			waiting |= !driver.at_us;
		}
		if (!waiting) {
			break;
		}
		vTaskDelay(1);
	}
	for (const auto& driver : drivers) {
		char detail[32];
		(void)std::snprintf(detail, sizeof detail, "%lld ms",
							(long long)(driver.at_us / 1000));
		output.report(driver.name, driver.at_us || driver.ready(),
					  driver.at_us ? detail : "after boot");
	}
}

void drivers_ready(Output& output) { report_driver_readiness(output, 0); }

#ifdef HWTEST_BENCHMARKS
constexpr uint32_t kBenchmarkSteps = 20;

template <size_t Aircraft>
void engine(Output& output) {
	// The idle tasks must run within the 5 s of the task watchdog.
	const benchmark::Result result =
		benchmark::engine(Aircraft, kBenchmarkSteps, [] { vTaskDelay(1); });
	output.print(
		"alerting %3u aircraft  step least %.1f mean %.1f most %.1f ms  "
		"alerting %u  heap lowest %u KB  stack left %u B\n",
		(unsigned)Aircraft, result.least_us / 1000.0, result.mean_us / 1000.0,
		result.most_us / 1000.0, (unsigned)result.peak_alerted,
		(unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT) / 1024),
		(unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

constexpr size_t kSkyAircraft = 200;
constexpr int kCycles = 5;

// The counters of the caches that both cores reach flash and external RAM
// through. What another task fetches meanwhile counts too.
benchmark::Cache caches() {
	return {REG_READ(EXTMEM_IBUS_ACS_CNT_REG),
			REG_READ(EXTMEM_IBUS_ACS_MISS_CNT_REG),
			REG_READ(EXTMEM_DBUS_ACS_CNT_REG),
			REG_READ(EXTMEM_DBUS_ACS_FLASH_MISS_CNT_REG),
			REG_READ(EXTMEM_DBUS_ACS_SPIRAM_MISS_CNT_REG)};
}

void stages(Output& output) {
	struct Stage {
		const char* name{};
		uint32_t benchmark::Cycle::* us{};
		benchmark::Cache benchmark::Cycle::* cache{};
		uint32_t least = UINT32_MAX, most = 0, total = 0;
		uint64_t accesses = 0, misses = 0;
		uint64_t data_accesses = 0, data_flash_misses = 0, data_ram_misses = 0;
	} parts[] = {{"detect", &benchmark::Cycle::detect_us,
				  &benchmark::Cycle::detect_cache},
				 {"altitude band", &benchmark::Cycle::altitude_us,
				  &benchmark::Cycle::altitude_cache},
				 {"track band", &benchmark::Cycle::track_us,
				  &benchmark::Cycle::track_cache},
				 {"speed band", &benchmark::Cycle::speed_us,
				  &benchmark::Cycle::speed_cache}};
	benchmark::Cycles cycles(kSkyAircraft, caches);
	benchmark::Cycle cycle;
	for (int i = 0; i < kCycles; i++) {
		vTaskDelay(1);
		cycle = cycles.measure_next_second();
		for (Stage& part : parts) {
			part.least = std::min(part.least, cycle.*part.us);
			part.most = std::max(part.most, cycle.*part.us);
			part.total += cycle.*part.us;
			if (part.cache) {
				part.accesses += (cycle.*part.cache).instruction_accesses;
				part.misses += (cycle.*part.cache).instruction_misses;
				part.data_accesses += (cycle.*part.cache).data_accesses;
				part.data_flash_misses += (cycle.*part.cache).data_flash_misses;
				part.data_ram_misses += (cycle.*part.cache).data_ram_misses;
			}
		}
	}
	output.print("%u aircraft, %u alerting, %u bands, ms\n",
				 (unsigned)kSkyAircraft, (unsigned)cycle.alerting_aircraft,
				 (unsigned)cycle.band_count);
	for (const Stage& part : parts) {
		output.print("%-14s least %6.1f  mean %6.1f  most %6.1f", part.name,
					 part.least / 1000.0, part.total / 1000.0 / kCycles,
					 part.most / 1000.0);
		if (part.cache) {
			output.print("  fetches %7.0f k  misses %6.0f  %.2f %%",
						 static_cast<double>(part.accesses) / 1000.0 / kCycles,
						 static_cast<double>(part.misses) / kCycles,
						 part.accesses
							 ? 100.0 * static_cast<double>(part.misses) /
								   static_cast<double>(part.accesses)
							 : 0);
			output.print(
				"\n%-14s data accesses %7.0f k  misses flash %6.0f  ram %6.0f",
				"", static_cast<double>(part.data_accesses) / 1000.0 / kCycles,
				static_cast<double>(part.data_flash_misses) / kCycles,
				static_cast<double>(part.data_ram_misses) / kCycles);
		}
		output.print("\n");
	}
	output.print(
		"heap lowest %u KB  stack left %u B\n",
		(unsigned)(heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT) / 1024),
		(unsigned)uxTaskGetStackHighWaterMark(nullptr));
}

void cycle(Output& output) {
	benchmark::Cycles cycles(kSkyAircraft);
	for (int i = 1; i <= kCycles; i++) {
		vTaskDelay(1);
		const benchmark::Cycle took = cycles.measure_next_second();
		output.print(
			"cycle %d  detect %.1f  altitude %.1f  track %.1f"
			"  speed %.1f  total %.1f ms  aircraft %u  alerting %u  bands "
			"%u\n",
			i, took.detect_us / 1000.0, took.altitude_us / 1000.0,
			took.track_us / 1000.0, took.speed_us / 1000.0,
			took.total_us() / 1000.0, (unsigned)kSkyAircraft,
			(unsigned)took.alerting_aircraft, (unsigned)took.band_count);
	}
}
#endif

constexpr Entry kEntries[] = {
	{"charger_identity", "The charger names itself", false, charger_identity},
	{"battery_identity", "The fuel gauge names itself", false,
	 battery_identity},
	{"barometer_identity", "The barometer names itself", false,
	 barometer_identity},
	{"fusb_identity", "The USB-C controller names itself", false,
	 fusb_identity},
	{"expander_identity", "The port expander answers", false,
	 expander_identity},
	{"drivers_ready", "Every driver is ready", false, drivers_ready},
#ifdef HWTEST_BENCHMARKS
	{"alerting_10", "DAIDALUS alerting only, 10 aircraft", true, engine<10>},
	{"alerting_50", "DAIDALUS alerting only, 50 aircraft", true, engine<50>},
	{"alerting_100", "DAIDALUS alerting only, 100 aircraft", true, engine<100>},
	{"alerting_200", "DAIDALUS alerting only, 200 aircraft", true, engine<200>},
	{"alerting_400", "DAIDALUS alerting only, 400 aircraft", true, engine<400>},
	{"stages",
	 "Detection and guidance bands, per-stage timings for 200 aircraft", true,
	 stages},
	{"cycle", "Five detection and guidance cycles with 200 aircraft", true,
	 cycle},
#endif
};

std::atomic_flag running = ATOMIC_FLAG_INIT;

struct Run {
	Run(const Entry& entry, Output output, SemaphoreHandle_t done)
		: entry(entry), output(output), done(done) {}

	const Entry& entry;
	Output output;
	SemaphoreHandle_t done;
};

void run_task(void* arg) {
	auto& run = *static_cast<Run*>(arg);
	run.entry.run(run.output);
	xSemaphoreGive(run.done);
	vTaskDelete(nullptr);
}
}

std::span<const Entry> entries() { return kEntries; }

RunResult run(const Entry& entry, std::FILE* stream) {
	if (running.test_and_set()) {
		return RunResult::busy;
	}
	StaticSemaphore_t storage;
	Run request{entry, Output(stream), xSemaphoreCreateBinaryStatic(&storage)};
	// Below everything that keeps the device working, and on one core, so a
	// run knows which idle task it keeps waiting.
	auto created =
		xTaskCreatePinnedToCore(run_task, "hwtest", kStackBytes, &request,
								tskIDLE_PRIORITY + 1, nullptr, 1);
	if (created == pdPASS) {
		xSemaphoreTake(request.done, portMAX_DELAY);
	}
	vSemaphoreDelete(request.done);
	running.clear();
	return created == pdPASS ? RunResult::done : RunResult::no_memory;
}

void run_boot_checks() {
	Output output(stdout);
	for (const Entry& entry : kEntries) {
		if (entry.benchmark) {
			continue;
		}
		if (entry.run == drivers_ready) {
			report_driver_readiness(output,
									esp_timer_get_time() + kReadyTimeoutUs);
		} else {
			entry.run(output);
		}
	}
}
}

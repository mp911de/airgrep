#pragma once

#include <bit>
#include <cstddef>
#include <limits>
#include <type_traits>

#include "adsbee/adsbee_protocol.hpp"
#include "baro/bmp580_i2c.hpp"
#include "battery/max17048_i2c.hpp"
#include "charger/bq25798_i2c.hpp"
#include "i2c_expander/tca9534_i2c.hpp"
#include "traffic/traffic_protocol.hpp"
#include "usb/fusb303_i2c.hpp"
#include "wifi/wifi_protocol.hpp"

// Version 1 copies the S3 ABI, including padding.
// A host that reads a State compiles this header too. A size, an alignment or
// one of the offsets below that differs between the two fails the build of
// the one that differs. The offsets are samples, not every field.
namespace usb::layout {
inline constexpr size_t kTargets = 512;

static_assert(std::endian::native == std::endian::little);
static_assert(sizeof(bool) == 1 && sizeof(int) == 4 && sizeof(unsigned) == 4);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(sizeof(battery::OptionalFloat) == 8);
static_assert(alignof(battery::OptionalFloat) == 4);
static_assert(offsetof(battery::OptionalFloat, value) == 0);
static_assert(offsetof(battery::OptionalFloat, valid) == 4);
static_assert(offsetof(battery::OptionalFloat, reserved) == 5);
static_assert(sizeof(Status) == 1 && sizeof(BatteryDetected) == 1);
static_assert(sizeof(adsbee::Ingestion) == 1);
static_assert(sizeof(traffic::Source) == 1);
static_assert(sizeof(traffic::FlarmAddressType) == 1);
static_assert(sizeof(charger::ControlMode) == 1 &&
			  sizeof(charger::Supply) == 1 && sizeof(charger::ChargeStop) == 1);
static_assert(sizeof(fusb::Role) == 4);

template <typename State, size_t Size>
inline constexpr bool kExposed =
	sizeof(State) == Size && alignof(State) == 8 &&
	std::is_trivially_copyable_v<State> && std::is_standard_layout_v<State>;

static_assert(kExposed<traffic::Target, 120>);
static_assert(kExposed<adsbee::Gnss, 168>);
static_assert(sizeof(traffic::Position) == 16 &&
			  alignof(traffic::Position) == 4);
static_assert(sizeof(traffic::Sample) == 1);
static_assert(offsetof(traffic::Position, sample) == 14);
static_assert(offsetof(traffic::Position, altitude_datum) == 15);
static_assert(kExposed<traffic::Tracked, 120 + 16 * traffic::kHistorySeconds>);
static_assert(offsetof(traffic::Tracked, past) == 120);
static_assert(kExposed<adsbee::StateData, 240>);
static_assert(kExposed<barometer::StateData, 88>);
static_assert(kExposed<battery::StateData, 72>);
static_assert(kExposed<charger::StateData, 288>);
static_assert(kExposed<expander::StateData, 64>);
static_assert(kExposed<fusb::StateData, 72>);
static_assert(kExposed<wifi::StateData, 144>);
static_assert(kExposed<adsbee::timesync::StateData, 56>);

static_assert(offsetof(traffic::Target, last_seen_us) == 8);
static_assert(offsetof(traffic::Target, latitude) == 36);
static_assert(offsetof(traffic::Target, flarm_frames) == 118);
static_assert(offsetof(adsbee::Gnss, epoch_ms) == 32);
static_assert(offsetof(adsbee::Gnss, module) == 100);
static_assert(offsetof(adsbee::Gnss, bbr) == 164);
static_assert(offsetof(adsbee::StateData, gnss) == 56);
static_assert(offsetof(adsbee::StateData, last_gnss_report_us) == 224);
static_assert(offsetof(adsbee::StateData, traffic_ingestion) == 232);
static_assert(offsetof(adsbee::StateData, gnss_ingestion) == 233);
static_assert(offsetof(barometer::StateData, pressure_hpa) == 52);
static_assert(offsetof(barometer::StateData, updated_us) == 64);
static_assert(offsetof(battery::StateData, soc) == 16);
static_assert(offsetof(battery::StateData, charge_duration_hours) == 36);
static_assert(offsetof(battery::StateData, updated_us) == 48);
static_assert(offsetof(charger::StateData, readings) +
				  offsetof(charger::PollReadings, status1) ==
			  83);
static_assert(offsetof(charger::StateData, updated_us) == 216);
static_assert(offsetof(charger::StateData, target) == 248);
static_assert(offsetof(expander::StateData, input) == 5);
static_assert(offsetof(expander::StateData, updated_us) == 40);
static_assert(offsetof(fusb::StateData, cc_limit_ma) == 44);
static_assert(offsetof(fusb::StateData, updated_us) == 48);
static_assert(offsetof(wifi::StateData, slider_enabled) == 2);
static_assert(offsetof(wifi::StateData, last_reported_config) == 5);
static_assert(offsetof(wifi::StateData, sent_us) == 104);
static_assert(offsetof(adsbee::timesync::StateData, last_sync_us) == 8);
static_assert(offsetof(adsbee::timesync::StateData, error_us) == 48);
}

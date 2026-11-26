#include "emulator.hpp"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <numbers>
#include <string_view>

#include "800x480-touch/display_settings.hpp"
#include "maneuvers.hpp"
#include "navigation/units.h"
#include "seeed-172x320-touch/display_settings.hpp"
#include "system/power_mode.h"
#include "traffic/traffic_table.hpp"
#include "usb/usb_commands.hpp"
#include "usb/usb_settings.hpp"

namespace device {
namespace {
using usb::protocol::Service;

constexpr size_t kFrameCapacity = UINT16_MAX;

usb::protocol::StateAccess find(void* owner, uint8_t id) {
	auto& device = *static_cast<EmulatedDevice*>(owner);
	auto component = static_cast<Component>(id);
	const auto bytes = device.states.bytes(component);
	if (component == Component::traffic) {
		return {
			bytes.size(), owner,
			[](void* context, std::span<uint8_t> out) {
				const auto data =
					static_cast<EmulatedDevice*>(context)->states.bytes(
						Component::traffic);
				std::memcpy(out.data(), data.data(), out.size());
				return true;
			},
			[](void* context, size_t offset, std::span<const uint8_t> input) {
				return static_cast<EmulatedDevice*>(context)
					->traffic_table()
					.write_bytes(offset, input);
			}};
	}
	return {bytes.size(), bytes.data(),
			[](void* state, std::span<uint8_t> out) {
				std::memcpy(out.data(), state, out.size());
				return true;
			},
			[](void* state, size_t offset, std::span<const uint8_t> input) {
				std::memcpy(static_cast<uint8_t*>(state) + offset, input.data(),
							input.size());
				return true;
			}};
}

bool update_traffic(void* owner, const uint8_t* payload, size_t count,
					int64_t now_us) {
	auto& device = *static_cast<EmulatedDevice*>(owner);
	std::vector<traffic::Target> targets(count);
	std::memcpy(targets.data(), payload, count * sizeof(traffic::Target));
	return device.traffic_table().upsert_batch(targets.data(), count, now_us);
}

bool update_gnss(void* owner, const uint8_t* payload, int64_t now_us) {
	auto& device = *static_cast<EmulatedDevice*>(owner);
	std::memcpy(&device.states.adsbee.gnss, payload, sizeof(adsbee::Gnss));
	device.states.adsbee.last_gnss_report_us = now_us;
	return true;
}

std::optional<usb::protocol::ListedTraffic> list_traffic(void* owner,
														 uint8_t* out,
														 size_t limit) {
	auto& device = *static_cast<EmulatedDevice*>(owner);
	if (device.states.traffic.empty()) {
		return std::nullopt;
	}
	const auto table = device.traffic_table();
	const adsbee::Gnss& gnss = device.states.adsbee.gnss;
	traffic::Reference own{gnss.latitude, gnss.longitude};
	std::vector<traffic::Tracked> listed(traffic::kListed);
	const uint16_t present = static_cast<uint16_t>(table.size());
	const size_t count = table.nearest(
		gnss.fix != adsbee::Fix::none ? &own : nullptr, listed.data(), limit);
	std::memcpy(out, listed.data(), count * sizeof(traffic::Tracked));
	return usb::protocol::ListedTraffic{present, count};
}

constexpr usb::protocol::Stored kStored[] = {
	usb::kSettings,
	usb::protocol::stored<display::touch_800x480::Settings>(
		display::touch_800x480::kSettingsKey),
	usb::protocol::stored<display::touch_172x320::Settings>(
		display::touch_172x320::kSettingsKey),
};

bool load_settings(void* owner, const char* key, uint8_t* out, size_t size) {
	auto& flash = *static_cast<EmulatedDevice*>(owner)->flash;
	const std::scoped_lock lock(flash.mutex);
	auto& stored = flash.values;
	const auto found = stored.find(key);
	if (found == stored.end() || found->second.size() != size) {
		return false;
	}
	std::memcpy(out, found->second.data(), size);
	return true;
}

bool store_settings(void* owner, const char* key, const uint8_t* blob,
					size_t size) {
	auto& flash = *static_cast<EmulatedDevice*>(owner)->flash;
	const std::scoped_lock lock(flash.mutex);
	flash.values[key].assign(blob, blob + size);
	return true;
}

bool erase_settings(void* owner) {
	auto& flash = *static_cast<EmulatedDevice*>(owner)->flash;
	const std::scoped_lock lock(flash.mutex);
	flash.values.clear();
	return true;
}

constexpr double kFlightCenterLatitude = 50.03, kFlightCenterLongitude = 8.57;
// The racetrack of the own ship lies this far east and north of it, so the
// flight begins where the nearest of that traffic passes.
constexpr double kRacetrackEast = 1.5, kRacetrackNorth = 1.0;
constexpr int32_t kAltitudeFt = 3000;
constexpr int64_t kInitialUptimeUs = 600'000'000;

void offset_from_flight_center(double east_nm, double north_nm, float& latitude,
							   float& longitude) {
	latitude = static_cast<float>(kFlightCenterLatitude +
								  north_nm / units::nm_per_degree_latitude);
	longitude =
		static_cast<float>(kFlightCenterLongitude +
						   east_nm / units::nm_per_degree_latitude /
							   std::cos(kFlightCenterLatitude *
										units::radians_per_degree<double>));
}

void position_at_bearing(double bearing_deg, double distance_nm,
						 float& latitude, float& longitude) {
	offset_from_flight_center(
		distance_nm * std::sin(bearing_deg * units::radians_per_degree<double>),
		distance_nm * std::cos(bearing_deg * units::radians_per_degree<double>),
		latitude, longitude);
}

struct OrbitingAircraft {
	const char* callsign;
	uint32_t address;
	traffic::Source source;
	uint8_t category, aircraft_type;
	double bearing_deg, distance_nm, lap_seconds;
	int32_t altitude_ft;
	int16_t rate_fpm;

	traffic::Target at(double seconds) const {
		traffic::Target target;
		target.address = address;
		target.source = source;
		put(target.callsign, callsign);
		target.mode_s_category = category;
		target.flarm_aircraft_type = aircraft_type;
		target.airborne = target.position_valid = target.baro_alt_valid =
			target.direction_valid = target.speed_valid =
				target.baro_rate_valid = true;
		const double bearing = bearing_deg + seconds / lap_seconds * 360;
		position_at_bearing(bearing, distance_nm, target.latitude,
							target.longitude);
		target.baro_alt_ft = altitude_ft;
		target.baro_rate_fpm = rate_fpm;
		const double ahead = lap_seconds > 0 ? 90 : -90;
		target.track_deg = static_cast<float>(
			std::fmod(std::fmod(bearing + ahead, 360.0) + 360, 360.0));
		target.speed_kt = static_cast<int16_t>(
			2 * std::numbers::pi * distance_nm / std::fabs(lap_seconds) * 3600);
		if (source == traffic::Source::flarm) {
			target.flarm_turn_rate_dps = static_cast<float>(360 / lap_seconds);
		}
		return target;
	}
};

constexpr OrbitingAircraft kOrbitingTraffic[] = {
	{"DEFGH", 0xDDE4A1, traffic::Source::flarm, 0, 1, 40, 2.4, 300, 3300, 150},
	{"DKXYZ", 0xDD9F31, traffic::Source::flarm, 0, 1, 250, 3.1, -420, 2400,
	 -200},
	{"", 0x2A7B9C, traffic::Source::tisb, 0, 0, 205, 4.2, -900, 2800, 0},
	{"DHXAB", 0x3D5E12, traffic::Source::adsb, 7, 0, 110, 4.6, 700, 1500, 0},
	{"DOBAL", 0xDD0F3B, traffic::Source::flarm, 0, 11, 150, 5.0, 5000, 3800, 0},
	{"RYR82QV", 0x4CA7E3, traffic::Source::adsb, 3, 0, 345, 7.5, -500, 5900,
	 900},
	{"DLH4YK", 0x3C6589, traffic::Source::adsb, 5, 0, 80, 11.0, 800, 10400,
	 -1400},
	{"DMAXI", 0x3FE2B0, traffic::Source::adsb, 1, 0, 190, 8.0, -1000, 4100, 0},
	{"D1234", 0xDDA311, traffic::Source::flarm, 0, 1, 275, 9.5, 1200, 2600,
	 180},
};

// The target that meets the own ship. Each maneuver is an aircraft of its
// own, so none jumps from the end of one encounter to the start of the next.
constexpr uint32_t kManeuverAddress = 0xA1B2C0;

traffic::Target maneuvering(double seconds) {
	const maneuvers::State state = maneuvers::target(seconds);
	const size_t flown = maneuvers::flown(
		static_cast<long>(std::floor(seconds / maneuvers::kEncounterS)));
	const maneuvers::Maneuver& maneuver = maneuvers::all()[flown];
	traffic::Target target;
	target.address = kManeuverAddress + static_cast<uint32_t>(flown);
	target.source = traffic::Source::adsb;
	put(target.callsign, maneuver.callsign);
	target.mode_s_category = maneuver.adsbee_emitter_category;
	target.airborne = target.position_valid = target.baro_alt_valid =
		target.direction_valid = target.speed_valid = target.baro_rate_valid =
			true;
	offset_from_flight_center(kRacetrackEast + state.east_nm,
							  kRacetrackNorth + state.north_nm, target.latitude,
							  target.longitude);
	target.baro_alt_ft =
		kAltitudeFt +
		static_cast<int32_t>(std::lround(state.relative_altitude_ft));
	target.baro_rate_fpm = static_cast<int16_t>(state.rate_fpm);
	target.track_deg = static_cast<float>(state.track_deg);
	target.speed_kt = static_cast<int16_t>(state.speed_kt);

	return target;
}
}

EmulatedDevice make_booted_service_device() {
	EmulatedDevice device;
	Hello& hello = device.hello;
	hello.service = Service::present;
	hello.mode = POWER_MODE_ON;
	hello.cpu_core0 = hello.cpu_core1 = hello.cpu_overall =
		AIRGREP_CPU_UNAVAILABLE;
	put(hello.firmware_version, "0.0.0-emulated");
	put(hello.variant, "emulated");
	put(hello.firmware_description, "airgrep emulated");
	put(hello.board, "Emulated");
	put(hello.chip, "This computer");
	return device;
}

void fly(EmulatedDevice& device, double elapsed_flight_s) {
	const int64_t now_us =
		kInitialUptimeUs + static_cast<int64_t>(elapsed_flight_s * 1e6);
	const maneuvers::State own = maneuvers::own(elapsed_flight_s);

	device.hello.uptime_seconds = static_cast<uint64_t>(now_us / 1'000'000);
	device.hello.internal_total = 372'115;
	device.hello.internal_free = device.hello.internal_minimum = 256'132;
	device.hello.psram_total = 8'388'608;
	device.hello.psram_free = device.hello.psram_minimum = 8'000'000;

	adsbee::StateData& link = device.states.adsbee;
	link.known = link.tx_attached = link.initialized = true;
	link.updated_us = link.reply_us = link.last_gnss_report_us = now_us;
	adsbee::Gnss& gnss = link.gnss;
	gnss.fix = adsbee::Fix::fix_3d;
	gnss.utc_known = true;
	gnss.quality = adsbee::FixQuality::gps;
	gnss.satellites_used = 11;
	gnss.used = 11;
	offset_from_flight_center(kRacetrackEast + own.east_nm,
							  kRacetrackNorth + own.north_nm, gnss.latitude,
							  gnss.longitude);
	gnss.altitude_ft = kAltitudeFt;
	gnss.speed_kt = static_cast<float>(own.speed_kt);
	gnss.track_deg = static_cast<float>(own.track_deg);
	gnss.hdop = 0.9f;
	const uint32_t clock =
		14 * 3600 + 32 * 60 + static_cast<uint32_t>(elapsed_flight_s);
	gnss.utc_hour = clock / 3600 % 24;
	gnss.utc_minute = clock / 60 % 60;
	gnss.utc_second = clock % 60;
	gnss.antenna = adsbee::AntennaStatus::ok;
	gnss.antenna_power = adsbee::AntennaPower::on;
	gnss.max_cno_dbhz = 42;

	std::ranges::fill(device.states.traffic, traffic::Target{});
	std::ranges::fill(device.history, traffic::Position{});
	auto table = device.traffic_table();
	const double encounter_start_s =
		std::floor(elapsed_flight_s / maneuvers::kEncounterS) *
		maneuvers::kEncounterS;
	const int available_age = static_cast<int>(
		std::clamp(std::floor(elapsed_flight_s), 0.0,
				   static_cast<double>(traffic::kHistorySeconds - 1)));
	for (int age = available_age; age >= 0; age--) {
		const double then = std::floor(elapsed_flight_s) - age;
		const int64_t then_us =
			kInitialUptimeUs + static_cast<int64_t>(then * 1e6);
		for (const OrbitingAircraft& flown : kOrbitingTraffic) {
			if (then >= 0) {
				table.upsert(flown.at(then), then_us);
			}
		}
		if (then >= encounter_start_s) {
			table.upsert(maneuvering(then), then_us);
		}
		table.sample(then_us);
	}
	for (const OrbitingAircraft& flown : kOrbitingTraffic) {
		table.upsert(flown.at(elapsed_flight_s), now_us);
	}
	table.upsert(maneuvering(elapsed_flight_s), now_us);
	device.history_head = table.history_head();

	device.states.barometer.status = ::Status::ok;
	device.states.barometer.identity_known =
		device.states.barometer.configured = true;
	device.states.barometer.osr = {2, 8, true};
	device.states.barometer.altitude_m =
		static_cast<float>(kAltitudeFt / units::feet_per_metre);
	device.states.barometer.pressure_hpa = 908;
	device.states.barometer.temperature_c = 21;
	device.states.barometer.updated_us = now_us;

	battery::StateData& gauge = device.states.battery;
	gauge.status = ::Status::ok;
	gauge.identity_known = gauge.configured = true;
	gauge.rate = -12;
	gauge.soc = static_cast<float>(std::max(78 - elapsed_flight_s / 300, 5.0));
	gauge.volts = 3.9f;
	gauge.charge_duration_hours = {.value = gauge.soc / -gauge.rate,
								   .valid = 1};
	gauge.updated_us = now_us;

	charger::StateData& charger = device.states.charger;
	charger.readings.status = ::Status::ok;
	charger.identity_known = charger.configured = charger.adc_valid = true;
	charger.readings.vbat_mv = 3900;
	charger.updated_us = now_us;

	expander::StateData& expander = device.states.expander;
	expander.status = ::Status::ok;
	expander.identity_known = expander.configured = true;
	expander.input.wifi = expander.input.power = true;
	expander.updated_us = now_us;

	wifi::StateData& wifi = device.states.wifi;
	wifi.status = ::Status::ok;
	wifi.slider_sampled = wifi.slider_enabled = wifi.reported_known = true;
	wifi.last_reported_config.enabled = true;
	put(wifi.last_reported_config.ssid, "airgrep-000000000000");
	put(wifi.last_reported_config.password, "airgrep-wifi");
	wifi.last_reported_config.channel = 11;
	wifi.reported_us = now_us;
}

void Emulator::reset_session() {
	decoder_.reset();
	input_.clear();
	output_.clear();
	device_.states.adsbee.traffic_ingestion =
		device_.states.adsbee.gnss_ingestion = adsbee::Ingestion::read;
}

void Emulator::write(std::span<const uint8_t> frame) {
	for (const uint8_t byte : frame) {
		input_.push_back(byte);
		if (decoder_.feed(byte, 0)) {
			reply();
		}
	}
}

void Emulator::reply() {
	std::vector<uint8_t> response(kFrameCapacity);
	auto immediate = usb::protocol::route(
		decoder_, response, [&] -> const Hello& { return device_.hello; });
	size_t length = 0;
	if (immediate) {
		length = immediate->size;
		if (immediate->after_send != Command::status) {
			restart_command = immediate->after_send;
		}
	} else {
		const usb::protocol::Services services{&device_,
											   find,
											   sizeof(traffic::Target),
											   sizeof(adsbee::Gnss),
											   update_traffic,
											   update_gnss,
											   sizeof(traffic::Tracked),
											   list_traffic,
											   kStored,
											   load_settings,
											   store_settings,
											   erase_settings};
		length = usb::protocol::execute_data(
			decoder_.command(), input_.data() + 6, input_.size() - 6,
			static_cast<int64_t>(device_.hello.uptime_seconds) * 1'000'000,
			services, response.data(), response.size());
	}
	output_.insert(output_.end(), response.begin(),
				   response.begin() + static_cast<std::ptrdiff_t>(length));
	decoder_.reset();
	input_.clear();
}

bool Emulator::read(std::vector<uint8_t>& bytes, size_t,
					std::chrono::milliseconds) {
	const bool any = !output_.empty();
	bytes.insert(bytes.end(), output_.begin(), output_.end());
	output_.clear();
	return any;
}

namespace {

class Flight : public Transport {
   public:
	explicit Flight(const std::shared_ptr<EmulatorFlash>& flash) {
		device_.flash = flash;
	}
	void reset_session() override { emulator_.reset_session(); }
	void write(std::span<const uint8_t> frame) override {
		const std::chrono::duration<double> flown =
			std::chrono::steady_clock::now() - began_;
		fly(device_, flown.count());
		emulator_.write(frame);
	}
	bool read(std::vector<uint8_t>& bytes, size_t wanted,
			  std::chrono::milliseconds timeout) override {
		return emulator_.read(bytes, wanted, timeout);
	}

   private:
	EmulatedDevice device_ = make_booted_service_device();
	Emulator emulator_{device_};
	std::chrono::steady_clock::time_point began_ =
		std::chrono::steady_clock::now();
};
}

std::unique_ptr<Transport> open_flight() {
	static const auto flash = std::make_shared<EmulatorFlash>();
	return std::make_unique<Flight>(flash);
}
}

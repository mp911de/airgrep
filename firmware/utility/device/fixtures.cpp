#include "fixtures.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstring>
#include <format>
#include <optional>
#include <ranges>
#include <vector>

#include "800x480-touch/display_settings.hpp"
#include "navigation/units.h"
#include "settings/settings_model.hpp"
#include "system/health.hpp"
#include "system/power_mode.h"
#include "traffic/traffic_table.hpp"

namespace device::fixtures {
namespace {
constexpr uint8_t kLight = 1, kLarge = 3, kHeavy = 5, kRotorcraft = 7,
				  kGlider = 9;
constexpr uint8_t kFlarmGlider = 1, kFlarmHelicopter = 3, kFlarmPiston = 8,
				  kFlarmJet = 9, kFlarmBalloon = 11;
constexpr int16_t kClimbFpm = 500;
// The charge status of the BQ25798 while it charges with constant current.
constexpr uint8_t kFastCharge = 3;

// A Target as the sketch states it: where it is from the own ship and where
// it flies against the track of the own ship. One without a track reports
// neither direction nor speed.
struct Seen {
	char callsign[10];
	uint32_t address;
	traffic::Source source;
	uint8_t category, aircraft_type;
	double bearing_deg, distance_nm;
	int32_t above_ft;
	int16_t rate_fpm;
	std::optional<double> track_deg;
	int16_t speed_kt = 0;
	float turn_dps = 0;
};

constexpr Seen kSketchTraffic[] = {
	{"N172SP", 0xA1B2C3, traffic::Source::adsb, kLight, 0, 300, 2.6, 200,
	 -kClimbFpm, 120, 100},
	{"D-EFGH", 0xDDE4A1, traffic::Source::flarm, 0, kFlarmGlider, 40, 1.4, 300,
	 kClimbFpm, 200, 70, 4},
	{"D-KXYZ", 0xDD9F31, traffic::Source::flarm, 0, kFlarmGlider, 250, 2.1,
	 -600, -kClimbFpm, 30, 60},
	{"", 0x2A7B9C, traffic::Source::tisb, 0, 0, 205, 2.9, -200, 0, {}},
	{"D-HXAB", 0x3D5E12, traffic::Source::adsb, kRotorcraft, 0, 110, 3.2, -1500,
	 0, 280, 90},
	{"RYR82QV", 0x4CA7E3, traffic::Source::adsb, kLarge, 0, 345, 3.6, 2500,
	 kClimbFpm, 230, 250},
	{"D-OBAL",
	 0xDD0F3B,
	 traffic::Source::flarm,
	 0,
	 kFlarmBalloon,
	 150,
	 3.4,
	 800,
	 0,
	 {}},
	{"DLH4YK", 0x3C6589, traffic::Source::adsb, kHeavy, 0, 80, 5.2, 2400,
	 -kClimbFpm, 250, 280},
	{"D-MAXI", 0x3FE2B0, traffic::Source::adsb, kLight, 0, 190, 6.0, 1100, 0,
	 10, 110},
	{"D-1234", 0xDDA311, traffic::Source::flarm, 0, kFlarmGlider, 275, 7.4,
	 -400, kClimbFpm, 90, 55},
};

// What the Target reported this long ago, having kept its speed and its turn.
traffic::Target reported(const Seen& seen, const adsbee::Gnss& own,
						 double ago_s) {
	traffic::Target target;
	target.address = seen.address;
	target.source = seen.source;
	put(target.callsign, seen.callsign);
	target.mode_s_category = seen.category;
	target.flarm_aircraft_type = seen.aircraft_type;
	target.airborne = target.position_valid = target.baro_alt_valid =
		target.baro_rate_valid = true;
	const double bearing = seen.bearing_deg * units::radians_per_degree<double>;
	double right_nm = seen.distance_nm * std::sin(bearing);
	double ahead_nm = seen.distance_nm * std::cos(bearing);
	double track_deg = 0;
	if (seen.track_deg) {
		const double heading =
			*seen.track_deg * units::radians_per_degree<double>;
		const double turn = seen.turn_dps * units::radians_per_degree<double>;
		const double speed_nm_s = seen.speed_kt / 3600.0;
		if (turn != 0) {
			right_nm += speed_nm_s / turn *
						(std::cos(heading) - std::cos(heading - turn * ago_s));
			ahead_nm += speed_nm_s / turn *
						(std::sin(heading - turn * ago_s) - std::sin(heading));
		} else {
			right_nm -= speed_nm_s * ago_s * std::sin(heading);
			ahead_nm -= speed_nm_s * ago_s * std::cos(heading);
		}
		track_deg = *seen.track_deg - seen.turn_dps * ago_s;
		target.direction_valid = target.speed_valid = true;
		target.track_deg = static_cast<float>(
			std::fmod(own.track_deg + track_deg + 720.0, 360.0));
		target.speed_kt = seen.speed_kt;
	}
	const double own_track = own.track_deg * units::radians_per_degree<double>;
	const double north_nm =
		ahead_nm * std::cos(own_track) - right_nm * std::sin(own_track);
	const double east_nm =
		ahead_nm * std::sin(own_track) + right_nm * std::cos(own_track);
	target.latitude = static_cast<float>(
		own.latitude + north_nm / units::nm_per_degree_latitude);
	target.longitude = static_cast<float>(
		own.longitude +
		east_nm / units::nm_per_degree_latitude /
			std::cos(own.latitude * units::radians_per_degree<double>));
	target.baro_alt_ft =
		own.altitude_ft + seen.above_ft -
		static_cast<int32_t>(std::lround(seen.rate_fpm * ago_s / 60));
	target.baro_rate_fpm = seen.rate_fpm;
	target.flarm_turn_rate_dps = seen.turn_dps;
	return target;
}

constexpr uint32_t kOwnAddress = 0x3D2A7F;

template <typename Settings>
void store(EmulatorFlash& flash, const char* key, const Settings& settings) {
	const auto* bytes = reinterpret_cast<const uint8_t*>(&settings);
	flash.values[key].assign(bytes, bytes + sizeof settings);
}

// The own ship of the sketch, D-EABC, as its owner stored it on the device.
settings::Settings identified() {
	settings::Settings stored = settings::defaults();
	stored.own_address = kOwnAddress;
	std::strncpy(stored.own_callsign, "DEABC", sizeof stored.own_callsign - 1);
	return stored;
}

// Where this leads after a second, from a place in degrees.
void move(float& latitude, float& longitude, double track_deg,
		  double speed_kt) {
	const double track = track_deg * units::radians_per_degree<double>;
	const double nm = speed_kt / 3600.0;
	const double north_deg =
		nm * std::cos(track) / units::nm_per_degree_latitude;
	const double east_deg =
		nm * std::sin(track) / units::nm_per_degree_latitude /
		std::cos(latitude * units::radians_per_degree<double>);
	latitude = static_cast<float>(latitude + north_deg);
	longitude = static_cast<float>(longitude + east_deg);
}

// One second later: the own ship and every Target have held their course,
// their turn and their climb, and every component has reported again.
void pass_second(EmulatedDevice& device) {
	constexpr int64_t second_us = 1'000'000;
	device.hello.uptime_seconds++;
	const int64_t now_us =
		static_cast<int64_t>(device.hello.uptime_seconds) * second_us;

	adsbee::StateData& link = device.states.adsbee;
	link.updated_us = link.reply_us = link.last_gnss_report_us = now_us;
	adsbee::Gnss& gnss = link.gnss;
	if (gnss.fix != adsbee::Fix::none) {
		move(gnss.latitude, gnss.longitude, gnss.track_deg, gnss.speed_kt);
	}
	const uint32_t clock =
		(gnss.utc_hour * 3600U + gnss.utc_minute * 60U + gnss.utc_second + 1U) %
		(24U * 3600U);
	gnss.utc_hour = static_cast<uint8_t>(clock / 3600);
	gnss.utc_minute = static_cast<uint8_t>(clock / 60 % 60);
	gnss.utc_second = static_cast<uint8_t>(clock % 60);

	std::vector<traffic::Target> flown;
	for (const traffic::Target& held : device.states.traffic) {
		if (held.last_seen_us) {
			flown.push_back(held);
		}
	}
	auto table = device.traffic_table();
	for (traffic::Target& target : flown) {
		if (target.direction_valid && target.speed_valid) {
			target.track_deg = std::fmod(
				target.track_deg + target.flarm_turn_rate_dps + 360.0F, 360.0F);
			move(target.latitude, target.longitude,
				 target.track_deg - target.flarm_turn_rate_dps / 2,
				 target.speed_kt);
		}
		target.baro_alt_ft += target.baro_rate_fpm / 60;
		table.upsert(target, now_us);
	}
	table.sample(now_us);
	device.history_head = table.history_head();

	device.states.barometer.updated_us += second_us;
	device.states.battery.updated_us += second_us;
	device.states.charger.updated_us += second_us;
	device.states.expander.updated_us += second_us;
	device.states.wifi.reported_us += second_us;
}

class Fixture : public Transport {
   public:
	Fixture(EmulatedDevice device,
			std::shared_ptr<const std::atomic<int64_t>> since_scene_ms)
		: device_(std::move(device)),
		  since_scene_ms_(std::move(since_scene_ms)) {}
	void reset_session() override { emulator_.reset_session(); }
	void write(std::span<const uint8_t> frame) override {
		const int64_t wanted_s =
			since_scene_ms_ ? since_scene_ms_->load() / 1000 : 0;
		for (; passed_s_ < wanted_s; passed_s_++) {
			pass_second(device_);
		}
		emulator_.write(frame);
	}
	bool read(std::vector<uint8_t>& bytes, size_t wanted,
			  std::chrono::milliseconds timeout) override {
		return emulator_.read(bytes, wanted, timeout);
	}

   private:
	EmulatedDevice device_;
	Emulator emulator_{device_};
	std::shared_ptr<const std::atomic<int64_t>> since_scene_ms_;
	int64_t passed_s_ = 0;
};

std::vector<Seen> sketch_traffic() {
	return {std::begin(kSketchTraffic), std::end(kSketchTraffic)};
}

Seen& find(std::vector<Seen>& traffic, std::string_view callsign) {
	return *std::ranges::find(traffic, callsign, [](const Seen& seen) {
		return std::string_view(seen.callsign);
	});
}

std::vector<Seen> only(std::initializer_list<std::string_view> callsigns) {
	std::vector<Seen> traffic = sketch_traffic();
	std::erase_if(traffic, [&](const Seen& seen) {
		return std::ranges::find(callsigns, std::string_view(seen.callsign)) ==
			   callsigns.end();
	});
	return traffic;
}

// One of the many Targets of the busy scene.
Seen crowd(int i) {
	struct Kind {
		uint8_t category, aircraft_type;
		int16_t speed_kt;
	};
	constexpr Kind kinds[] = {{kLight, kFlarmPiston, 100},
							  {kGlider, kFlarmGlider, 60},
							  {kLarge, kFlarmJet, 260},
							  {kLight, kFlarmPiston, 120},
							  {kRotorcraft, kFlarmHelicopter, 90}};
	constexpr int16_t rates[] = {0, kClimbFpm, -kClimbFpm};
	const Kind& kind = kinds[i % 5];
	const bool flarm = i % 3 == 0;
	Seen seen{
		.callsign = {},
		.address = static_cast<uint32_t>(0x3D1000 + i * 0x1A7),
		.source = flarm ? traffic::Source::flarm : traffic::Source::adsb,
		.category = flarm ? uint8_t{0} : kind.category,
		.aircraft_type = flarm ? kind.aircraft_type : uint8_t{0},
		.bearing_deg = static_cast<double>((i * 47 + 20) % 360),
		.distance_nm = 3 + 13.5 * std::sqrt(std::fmod(i * 5.3, 12.0) / 12),
		.above_ft = (i * 7 % 40 - 12) * 100,
		.rate_fpm = rates[i % 3],
		.track_deg = static_cast<double>(i * 83 % 360),
		.speed_kt = kind.speed_kt};
	put(seen.callsign, std::format("D-E{:c}{:c}{:c}", 'A' + i % 26,
								   'A' + i * 7 % 26, 'A' + i * 11 % 26));
	return seen;
}

// The device of the sketch with this traffic around it, each Target with the
// past of its last minute.
EmulatedDevice among(std::span<const Seen> traffic) {
	EmulatedDevice device = make_booted_service_device();
	fly(device, 0);
	put(device.hello.board, "S3 Plus");
	put(device.hello.firmware_version, "0.1.0");
	put(device.states.wifi.last_reported_config.ssid, "airgrep-a1b2c3d4e5f6");
	put(device.states.wifi.last_reported_config.password, "glide-9x2k-tower");
	device.states.battery.charge_duration_hours = {.value = 250 / 60.0f,
												   .valid = 1};

	const int64_t now_us =
		static_cast<int64_t>(device.hello.uptime_seconds) * 1'000'000;
	std::ranges::fill(device.states.traffic, traffic::Target{});
	std::ranges::fill(device.history, traffic::Position{});
	auto table = device.traffic_table();
	for (int ago_s = traffic::kHistorySeconds - 1; ago_s >= 0; ago_s--) {
		const int64_t then_us = now_us - int64_t{ago_s} * 1'000'000;
		for (const Seen& seen : traffic) {
			table.upsert(reported(seen, device.states.adsbee.gnss, ago_s),
						 then_us);
		}
		table.sample(then_us);
	}
	device.history_head = table.history_head();
	return device;
}

bool number(std::string_view text, uint8_t& value) {
	const auto [end, error] =
		std::from_chars(text.data(), text.data() + text.size(), value);
	return error == std::errc{} && end == text.data() + text.size();
}

// The stored display settings a word of a fixture name asks for.
void choose(display::touch_800x480::Settings& settings, std::string_view word) {
	if (word == "dark") {
		settings.theme = display::touch_800x480::Theme::dark;
	} else if (word == "tracks") {
		settings.tracks = 1;
	} else if (word == "simple-symbols") {
		settings.symbol_style = display::touch_800x480::SymbolStyle::simple;
	} else if (word == "conflict-indicator") {
		settings.conflict_indicator = 1;
	} else if (word.starts_with("language=") && word.size() == 11) {
		word.substr(9).copy(settings.language, 2);
	} else {
		uint8_t range_nm = 0;
		const auto range =
			word.starts_with("range=") && number(word.substr(6), range_nm)
				? display::radar_range_index(range_nm)
				: std::nullopt;
		if (!range) {
			throw Error(std::format("No display setting is called {}", word));
		}
		settings.radar_range_index = *range;
	}
}

// From ahead on the left, 300 ft below and level, straight at where the own
// ship will be: well clear is lost within a minute, which is a corrective
// alert.
void approach(Seen& glider, double distance_nm) {
	glider.bearing_deg = 330;
	glider.distance_nm = distance_nm;
	glider.above_ft = -300;
	glider.rate_fpm = 0;
	glider.track_deg = 110;
	glider.speed_kt = 70;
}

struct Named {
	std::string_view name;
	EmulatedDevice (*device)();
};
EmulatedDevice quiet() { return among({}); }

constexpr Named kFixtures[] = {
	{"normal", normal},     {"advisory", advisory}, {"alert", alert},
	{"no-fix", no_fix},     {"own-ship", own_ship}, {"busy", busy},
	{"beyond", beyond},     {"conflict", conflict}, {"corrective", corrective},
	{"charging", charging}, {"causes", causes},     {"preventive", preventive},
	{"recovery", recovery}, {"quiet", quiet}};
}

EmulatedDevice normal() { return among(kSketchTraffic); }

EmulatedDevice advisory() {
	std::vector<Seen> traffic = sketch_traffic();
	// Slowly across the way of the own ship: well clear is lost in half a
	// minute, which is a corrective alert and no warning yet.
	Seen& glider = find(traffic, "D-EFGH");
	glider.track_deg = 326;
	glider.turn_dps = 0;
	glider.rate_fpm = 200;
	return among(traffic);
}

EmulatedDevice alert() {
	std::vector<Seen> traffic = sketch_traffic();
	// Both outside well clear and closing: a warning some seconds ahead.
	Seen& first = find(traffic, "N172SP");
	first.distance_nm = 1.5;
	first.bearing_deg = 315;
	first.track_deg = 60;
	Seen& second = find(traffic, "D-EFGH");
	second.distance_nm = 1.8;
	second.above_ft = 100;
	second.track_deg = 276;
	second.turn_dps = 0;
	approach(find(traffic, "D-KXYZ"), 2.8);
	return among(traffic);
}

EmulatedDevice recovery() {
	std::vector<Seen> traffic = sketch_traffic();
	// Inside well clear, 0.66 NM and 450 ft: no track leads out in time.
	Seen& first = find(traffic, "N172SP");
	first.distance_nm = 0.6;
	first.bearing_deg = 315;
	Seen& second = find(traffic, "D-EFGH");
	second.distance_nm = 0.9;
	second.above_ft = 100;
	return among(traffic);
}

EmulatedDevice no_fix() {
	EmulatedDevice device = normal();
	device.states.adsbee.gnss.fix = adsbee::Fix::none;
	return device;
}

EmulatedDevice own_ship() {
	std::vector<Seen> traffic = sketch_traffic();
	traffic.push_back({.callsign = "D-EABC",
					   .address = kOwnAddress,
					   .source = traffic::Source::adsb,
					   .category = kLight,
					   .aircraft_type = 0,
					   .bearing_deg = 0,
					   .distance_nm = 0,
					   .above_ft = 0,
					   .rate_fpm = 0,
					   .track_deg = 0,
					   .speed_kt = 90});
	return among(traffic);
}

EmulatedDevice busy() {
	std::vector<Seen> traffic = sketch_traffic();
	for (int i = 0; traffic.size() < 32; i++) {
		traffic.push_back(crowd(i));
	}
	return among(traffic);
}

EmulatedDevice beyond() {
	std::vector<Seen> traffic = only({"D-EFGH", "N172SP"});
	find(traffic, "N172SP").track_deg = 60;
	return among(traffic);
}

EmulatedDevice conflict() {
	std::vector<Seen> traffic =
		only({"D-EFGH", "N172SP", "D-KXYZ", "D-HXAB", "D-OBAL"});
	Seen& first = find(traffic, "N172SP");
	first.distance_nm = 1.7;
	first.bearing_deg = 315;
	first.above_ft = 400;
	first.rate_fpm = 0;
	first.track_deg = 60;
	approach(find(traffic, "D-KXYZ"), 2.6);
	Seen& hovering = find(traffic, "D-HXAB");
	hovering.bearing_deg = 0;
	hovering.above_ft = -300;
	hovering.speed_kt = 0;
	Seen& balloon = find(traffic, "D-OBAL");
	balloon.bearing_deg = 180;
	balloon.distance_nm = 3.0;
	return among(traffic);
}

EmulatedDevice corrective() {
	std::vector<Seen> traffic = only({"D-EFGH"});
	Seen& crossing_glider = find(traffic, "D-EFGH");
	crossing_glider.bearing_deg = 14;
	crossing_glider.distance_nm = 2.4;
	crossing_glider.above_ft = 200;
	crossing_glider.rate_fpm = -kClimbFpm;
	// Head on at 70 kt is a warning. Across the nose at 50 kt, well clear is
	// lost in three quarters of a minute.
	crossing_glider.track_deg = 270;
	crossing_glider.speed_kt = 50;
	crossing_glider.turn_dps = 0;
	return among(traffic);
}

EmulatedDevice preventive() {
	std::vector<Seen> traffic = only({"D-EFGH"});
	Seen& glider = find(traffic, "D-EFGH");
	glider.bearing_deg = 14;
	glider.distance_nm = 2.4;
	glider.above_ft = 600;
	glider.rate_fpm = 0;
	glider.track_deg = 270;
	glider.speed_kt = 50;
	glider.turn_dps = 0;
	return among(traffic);
}

EmulatedDevice charging() {
	EmulatedDevice device = normal();
	device.hello.mode = POWER_MODE_CHARGER;
	battery::StateData& gauge = device.states.battery;
	gauge.soc = 64;
	gauge.rate = 27;
	gauge.charge_duration_hours = {.value = 80 / 60.0f, .valid = 1};
	device.states.charger.readings.status1.charge = kFastCharge;
	return device;
}

EmulatedDevice causes() {
	EmulatedDevice device = no_fix();
	device.hello.health = health::kAdsbee | health::kAntennaOpen | health::kCpu;
	device.hello.cpu_core0 = 87;
	device.states.adsbee.gnss.antenna = adsbee::AntennaStatus::open;
	return device;
}

namespace {
const Named& named(std::string_view name) {
	const auto* fixture = std::ranges::find(kFixtures, name, &Named::name);
	if (fixture == std::end(kFixtures)) {
		throw Error(std::format("No fixture is called {}", name));
	}
	return *fixture;
}
}

adsbee::Gnss gnss_of(std::string_view name) {
	return named(name).device().states.adsbee.gnss;
}

std::unique_ptr<Transport> open(
	std::string_view name,
	std::shared_ptr<const std::atomic<int64_t>> since_scene_ms) {
	auto words =
		std::views::split(name, ' ') | std::views::transform([](auto&& word) {
			return std::string_view(word);
		});
	auto word = words.begin();
	if (word == words.end() || (*word).empty()) {
		throw Error("No fixture name");
	}
	const Named* fixture = &named(*word);
	EmulatedDevice device = fixture->device();
	display::touch_800x480::Settings settings;
	settings.tracks = 0;
	// The scene own-ship is about a device that does not know its own ship.
	settings::Settings of_device = std::string_view(fixture->name) == "own-ship"
									   ? settings::defaults()
									   : identified();
	while (++word != words.end()) {
		if ((*word).starts_with("zone=")) {
			of_device.tz_detect = 0;
			put(of_device.zone, (*word).substr(5));
		} else if (*word == "no-own-ship") {
			of_device.own_address = SETTINGS_NO_ADDRESS;
			of_device.own_callsign[0] = 0;
		} else {
			choose(settings, *word);
		}
	}
	store(*device.flash, display::touch_800x480::kSettingsKey, settings);
	store(*device.flash, SETTINGS_NVS_KEY, of_device);
	return std::make_unique<Fixture>(std::move(device),
									 std::move(since_scene_ms));
}
}

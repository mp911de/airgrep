#include "inspect.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <format>
#include <type_traits>
#include <utility>

#include "system/power_mode.h"

namespace device {
namespace {
enum class Severity { advisory, invalid };

class Report {
   public:
	explicit Report(int64_t now_us) : now_us_(now_us) {}
	Inspection result;

	void begin(const std::string& component) { component_ = component; }

	void squawk(const char* name, int value) {
		const std::string digits = std::to_string(value);
		const bool valid =
			value == -1 ||
			(digits.size() <= 4 &&
			 digits.find_first_not_of("01234567") == std::string::npos);
		line(name, digits, valid ? "" : "expected four octal digits or -1");
	}

	void factor(const char* name, unsigned value, bool valid) {
		line(name, std::to_string(value),
			 valid ? "" : "invalid decoded factor");
	}

	void number(const char* name, int64_t value, int64_t low, int64_t high,
				Severity severity = Severity::advisory) {
		line(name, std::to_string(value),
			 value < low || value > high
				 ? std::format("outside {} to {}", low, high)
				 : "",
			 severity);
	}

	template <typename Enum>
		requires std::is_enum_v<Enum>
	void number(const char* name, Enum value, int64_t low, int64_t high,
				Severity severity = Severity::advisory) {
		number(name, std::to_underlying(value), low, high, severity);
	}

	template <typename Number>
	void shown(const char* name, Number value) {
		line(name, std::format("{}", value), "");
	}

	void real(const char* name, float value, float low, float high) {
		const bool finite = std::isfinite(value);
		line(name, std::format("{}", value),
			 !finite ? "not a finite number"
			 : value < low || value > high
				 ? std::format("outside {} to {}", low, high)
				 : "",
			 !finite ? Severity::invalid : Severity::advisory);
	}

	void time(const char* name, int64_t us) {
		line(name, std::to_string(us),
			 us < 0         ? "negative"
			 : us > now_us_ ? std::format("ahead of the clock, {}", now_us_)
							: "",
			 us < 0 ? Severity::invalid : Severity::advisory);
	}

	template <size_t Size>
	void text(const char* name, const char (&field)[Size]) {
		const size_t length = strnlen(field, Size);
		std::string value(field, length);
		const bool printable = std::ranges::all_of(
			value, [](unsigned char c) { return c >= 0x20 && c < 0x7f; });
		line(name, std::format("\"{}\"", printable ? value : "?"),
			 length == Size ? "no end within the field"
			 : !printable   ? "not printable"
							: "",
			 length == Size ? Severity::invalid : Severity::advisory);
	}

	template <size_t WireBytes, typename Flags>
	void flag_bytes(const char* name, const Flags& value) {
		static_assert(sizeof(Flags) == WireBytes &&
					  std::has_unique_object_representations_v<Flags>);
		flags(name, bytes_of(value));
	}

	void flags(const char* name, const bool& flag) {
		flags(name, bytes_of(flag));
	}

	// Bools that follow each other in a struct, read as the bytes they are:
	// copying one would hide a byte that is neither 0 nor 1.
	void flags(const char* name, std::span<const uint8_t> bytes) {
		std::string value;
		bool valid = true;
		for (const uint8_t byte : bytes) {
			valid = valid && byte <= 1;
			value += byte <= 1 ? static_cast<char>('0' + byte) : '?';
		}
		line(name, value, valid ? "" : "a bool that is neither 0 nor 1",
			 Severity::invalid);
	}

	template <size_t Size>
	void raw(const char* name, const uint8_t (&bytes)[Size]) {
		std::string value;
		for (uint8_t byte : bytes) {
			value += std::format("{:02x}", byte);
		}
		line(name, value, "");
	}

	void past(const char* name,
			  const traffic::Position (&seconds)[traffic::kHistorySeconds]) {
		std::string value;
		bool valid = true;
		for (const traffic::Position& second : seconds) {
			const auto sample =
				*reinterpret_cast<const uint8_t*>(&second.sample);
			char marker = '?';
			switch (static_cast<traffic::Sample>(sample)) {
				case traffic::Sample::none:
					marker = '-';
					break;
				case traffic::Sample::held:
					marker = 'H';
					break;
				case traffic::Sample::fresh:
					marker = 'F';
					break;
			}
			const bool valid_position = std::isfinite(second.latitude) &&
										std::isfinite(second.longitude);
			const bool valid_datum =
				second.altitude_datum <= traffic::AltitudeDatum::unavailable;
			valid = valid && marker != '?' && valid_position && valid_datum;
			value += valid_position && valid_datum ? marker : '?';
		}
		line(name, value, valid ? "" : "a second that is no position",
			 Severity::invalid);
	}

	template <typename Choice>
	void choice(const char* name, Choice value, int count) {
		number(name, static_cast<int64_t>(value), 0, count - 1,
			   Severity::invalid);
	}

   private:
	void line(const char* name, const std::string& value,
			  const std::string& problem,
			  Severity severity = Severity::advisory) {
		result.fields++;
		const std::string field =
			std::format("{}.{} = {}", component_, name, value);
		result.text += (problem.empty() ? "  " : "!!") + field;
		if (!problem.empty()) {
			result.problems++;
			result.text += "   <-- " + problem;
			if (severity == Severity::invalid &&
				result.first_invalid_field.empty()) {
				result.first_invalid_field = field + ", " + problem;
			}
		}
		result.text += '\n';
	}

	int64_t now_us_;
	std::string component_;
};

#define FIELD(member) #member, state.member

template <typename State>
void driver(Report& report, const State& state, Status status) {
	report.choice("status", status, 3);
	report.time(FIELD(updated_us));
	report.time(FIELD(error_us));
	report.shown(FIELD(last_error));
	report.flags(FIELD(identity_known));
	report.flags(FIELD(configured));
}

void inspect(Report& report, const Hello& state) {
	report.begin("hello");
	report.number(FIELD(uptime_seconds), 0, int64_t{10} * 365 * 86400,
				  Severity::invalid);
	report.text(FIELD(firmware_version));
	report.number(FIELD(internal_free), 0, state.internal_total);
	report.number(FIELD(internal_minimum), 0, state.internal_free);
	report.number(FIELD(internal_total), 1, UINT32_MAX);
	report.number(FIELD(psram_free), 0, state.psram_total);
	report.number(FIELD(psram_minimum), 0, state.psram_free);
	report.shown(FIELD(psram_total));
	report.shown(FIELD(health));
	report.choice(FIELD(mode), POWER_MODE_OFF + 1);
	for (auto [name, load] : {std::pair{"cpu_core0", state.cpu_core0},
							  {"cpu_core1", state.cpu_core1},
							  {"cpu_overall", state.cpu_overall}}) {
		if (load == AIRGREP_CPU_UNAVAILABLE) {
			report.shown(name, "unavailable");
		} else {
			report.number(name, load, 0, 100);
		}
	}
	report.choice(FIELD(service), 3);
	report.number(FIELD(reserved), 0, 0);
	report.text(FIELD(firmware_description));
	report.text(FIELD(board));
	report.text(FIELD(chip));
}

void inspect(Report& report, const adsbee::Gnss& state) {
	report.choice(FIELD(fix), static_cast<int>(adsbee::Fix::fix_3d) + 1);
	report.number(FIELD(quality), -1, 8);
	report.number("satellites", state.satellites_used, -1, 127);
	report.real(FIELD(latitude), -90, 90);
	report.real(FIELD(longitude), -180, 180);
	report.number(FIELD(altitude_ft), -2000, 100000);
	report.real(FIELD(speed_kt), 0, 2000);
	report.real(FIELD(track_deg), 0, 360);
	report.real(FIELD(hdop), -1, 100);
	report.flags(FIELD(utc_known));
	report.number(FIELD(utc_hour), 0, 23);
	report.number(FIELD(utc_minute), 0, 59);
	report.number(FIELD(utc_second), 0, 60);
	report.shown(FIELD(epoch_ms));
	report.number(FIELD(antenna), -1, 4);
	report.number(FIELD(antenna_power), -1, 2);
	report.number(FIELD(jamming), -1, 3);
	report.number("max_cno", state.max_cno_dbhz, -1, 99);
	for (auto [name, count] : {std::pair{"seen", state.seen},
							   {"no_signal", state.no_signal},
							   {"searching", state.searching},
							   {"acquired", state.acquired},
							   {"unusable", state.unusable},
							   {"locked", state.locked},
							   {"ephemeris", state.ephemeris},
							   {"used", state.used},
							   {"healthy", state.healthy},
							   {"unhealthy", state.unhealthy},
							   {"sbas_satellites", state.sbas_satellites}}) {
		report.number(name, count, -1, 255);
	}
	report.text(FIELD(sbas_mode));
	report.text(FIELD(sbas_system));
	report.flags(FIELD(sbas_corrections));
	report.shown(FIELD(prc_cm));
	report.shown(FIELD(ic_cm));
	report.text(FIELD(module));
	report.text(FIELD(software));
	report.text(FIELD(hardware));
	report.flags(FIELD(bbr));
}

void inspect(Report& report, const adsbee::StateData& state) {
	report.begin("adsbee");
	report.flags(FIELD(known));
	report.flags(FIELD(tx_attached));
	report.flags(FIELD(initialized));
	report.time(FIELD(updated_us));
	report.time(FIELD(reply_us));
	report.time(FIELD(error_us));
	report.shown(FIELD(last_error));
	report.shown(FIELD(telemetry_lines));
	report.shown(FIELD(crc_failures));
	report.shown(FIELD(parity_errors));
	report.shown(FIELD(framing_errors));
	report.shown(FIELD(overruns));
	report.time("gnss_us", state.last_gnss_report_us);
	report.choice(FIELD(traffic_ingestion), 2);
	report.choice(FIELD(gnss_ingestion), 2);
	report.begin("adsbee.gnss");
	inspect(report, state.gnss);
}

void inspect(Report& report, const traffic::Target& state, size_t slot,
			 const char* list = "traffic") {
	report.begin(std::format("{}[{}]", list, slot));
	report.number(FIELD(address), 0, 0xFFFFFF);
	report.choice(FIELD(source), traffic::source_count);
	report.time("seen_us", state.last_seen_us);
	report.text(FIELD(callsign));
	report.squawk(FIELD(squawk));
	static_assert(offsetof(traffic::Target, gnss_rate_valid) -
					  offsetof(traffic::Target, airborne) ==
				  7);
	report.flags(
		"airborne and valid flags",
		bytes_of(state).subspan(offsetof(traffic::Target, airborne), 8));
	report.real(FIELD(latitude), -90, 90);
	report.real(FIELD(longitude), -180, 180);
	report.number(FIELD(baro_alt_ft), -5000, 150000);
	report.number(FIELD(gnss_alt_ft), -5000, 150000);
	report.real(FIELD(track_deg), 0, 360);
	report.number(FIELD(speed_kt), 0, 5000);
	report.shown(FIELD(baro_rate_fpm));
	report.shown(FIELD(gnss_rate_fpm));
	report.number(FIELD(rssi_dbm), -128, 0);
	report.shown(FIELD(signal_quality_db));
	report.shown("flags", state.mode_s_flags);
	report.number("category", state.mode_s_category, 0, 31);
	report.shown("nicnac", state.mode_s_nicnac);
	report.shown("acdims", state.mode_s_acdims);
	report.number("adsb_version", state.mode_s_adsb_version, 0, 7);
	report.shown("squitter_fps", state.mode_s_squitter_fps);
	report.shown("es_fps", state.mode_s_es_fps);
	report.choice("id_type", state.flarm_id_type, 2);
	report.flags("stealth", state.flarm_stealth);
	report.flags("no_track", state.flarm_no_track);
	report.number("aircraft_type", state.flarm_aircraft_type, 0, 31);
	report.number("movement", state.flarm_movement, 0, 3);
	report.number("urgency", state.flarm_urgency, 0, 3);
	report.real("turn_rate_dps", state.flarm_turn_rate_dps, -360, 360);
	report.real("acc_pos_hor_m", state.flarm_acc_pos_hor_m, 0, 1e6f);
	report.real("acc_pos_ver_m", state.flarm_acc_pos_ver_m, 0, 1e6f);
	report.real("acc_vel_mps", state.flarm_acc_vel_mps, 0, 1e6f);
	report.shown("sil", state.flarm_sil);
	report.shown("sda", state.flarm_sda);
	report.shown("nic", state.flarm_nic);
	report.shown("fix_secs", state.flarm_fix_secs);
	report.shown("age_ms", state.flarm_age_ms);
	report.shown("version", state.flarm_version);
	report.shown("version_max", state.flarm_version_max);
	report.shown("frames", state.flarm_frames);
}

void inspect(Report& report, const barometer::StateData& state) {
	report.begin("barometer");
	driver(report, state, state.status);
	report.shown(FIELD(chip_id));
	report.shown(FIELD(raw_status));
	report.shown(FIELD(raw_odr));
	report.shown(FIELD(raw_osr));
	report.shown(FIELD(raw_iir));
	report.shown(FIELD(raw_int));
	report.shown(FIELD(raw_fifo));
	report.shown(FIELD(raw_osr_eff));
	report.raw(FIELD(data));
	report.flag_bytes<3>(FIELD(nvm));
	report.number(FIELD(odr.mode), 0, 3);
	report.number(FIELD(odr.rate), 0, 31);
	report.flags(FIELD(odr.deep_disabled));
	const auto osr = [&](unsigned value) {
		return (!state.configured && value == 0) ||
			   (value <= 128 && std::has_single_bit(value));
	};
	const auto iir = [](unsigned value) {
		return value <= 127 && std::has_single_bit(value + 1);
	};
	report.factor(FIELD(osr.temperature), osr(state.osr.temperature));
	report.factor(FIELD(osr.pressure), osr(state.osr.pressure));
	report.flags(FIELD(osr.pressure_enabled));
	report.factor(FIELD(iir.temperature), iir(state.iir.temperature));
	report.factor(FIELD(iir.pressure), iir(state.iir.pressure));
	report.real(FIELD(pressure_hpa), 0, 1300);
	report.real(FIELD(temperature_c), -60, 100);
	report.real(FIELD(altitude_m), -1000, 50000);
}

void inspect(Report& report, const battery::StateData& state) {
	report.begin("battery");
	driver(report, state, state.status);
	report.shown(FIELD(raw_vcell));
	report.shown(FIELD(raw_soc));
	report.shown(FIELD(raw_status));
	report.shown(FIELD(raw_version));
	report.shown(FIELD(raw_crate));
	report.real(FIELD(volts), 0, 6);
	report.real(FIELD(soc), 0, 256);
	report.real(FIELD(rate), -1000, 1000);
	report.flags(FIELD(low));
	report.shown(FIELD(version));
	report.flag_bytes<6>(FIELD(alerts));
	const auto hours = state.charge_duration_hours;
	report.number("charge_duration_hours.valid", hours.valid, 0, 1,
				  Severity::invalid);
	if (hours.valid == 1) {
		report.real("charge_duration_hours", hours.value, 0, 1e6f);
	}
}

void inspect(Report& report, const charger::StateData& state) {
	report.begin("charger");
	driver(report, state, state.readings.status);
	report.raw("raw", state.readings.raw);
	report.shown("identity.part", state.readings.identity.part);
	report.shown("identity.revision", state.readings.identity.revision);
	report.flag_bytes<7>("status0", state.readings.status0);
	report.number("status1.charge", state.readings.status1.charge, 0, 7);
	report.number("status1.vbus", state.readings.status1.vbus, 0, 15);
	report.flags("status1.detection_done",
				 state.readings.status1.detection_done);
	report.number("status2.optimizer", state.readings.status2.optimizer, 0, 3);
	report.flags("status2 flags",
				 bytes_of(state.readings.status2)
					 .subspan(offsetof(charger::Status2, thermal), 3));
	report.flag_bytes<7>("status3", state.readings.status3);
	report.flag_bytes<5>("status4", state.readings.status4);
	report.flag_bytes<8>("fault0", state.readings.fault0);
	report.flag_bytes<8>("fault_flag0", state.readings.fault_flag0);
	report.flag_bytes<5>("fault1", state.readings.fault1);
	report.flag_bytes<5>("fault_flag1", state.readings.fault_flag1);
	report.flag_bytes<8>("flag0", state.readings.flag0);
	report.flag_bytes<2>("control0", state.readings.control0);
	report.number("control1.watchdog_ms", state.readings.control1.watchdog_ms,
				  0, 160000);
	report.flags("control1.reset", state.readings.control1.reset);
	report.flag_bytes<3>("control5", state.readings.control5);
	report.flags("adc_control.enabled", state.readings.adc_control.enabled);
	report.flags("adc_control.continuous",
				 state.readings.adc_control.continuous);
	report.number("adc_control.sample_ms", state.readings.adc_control.sample_ms,
				  0, 10000);
	report.number("charge_current_ma", state.readings.charge_current_ma, 0,
				  5000);
	report.number("input_current_ma", state.readings.input_current_ma, 0, 3300);
	report.number("thermal_limit_c", state.readings.thermal_limit_c, 0, 120);
	report.number("ibus_ma", state.readings.ibus_ma, -5000, 5000);
	report.number("ibat_ma", state.readings.ibat_ma, -8000, 8000);
	report.number("vbus_mv", state.readings.vbus_mv, 0, 30000);
	report.number("vbat_mv", state.readings.vbat_mv, 0, 20000);
	report.number("vsys_mv", state.readings.vsys_mv, 0, 24000);
	report.choice(FIELD(battery_detected), 4);
	report.number(FIELD(battery_detection_vbat_mv), 0, 20000);
	report.flags(FIELD(battery_detection_vbat_valid));
	report.real(FIELD(ts_percent), 0, 100);
	report.real(FIELD(die_c), -60, 200);
	report.time(FIELD(adc_ready_us));
	report.flags(FIELD(adc_valid));
	report.choice(FIELD(target.mode), 2);
	report.flags("target.decided", state.target.target_available);
	report.choice(FIELD(target.supply), 4);
	report.number("target.vbus_type", state.target.detected_vbus_type, 0, 15);
	report.time("target.waiting_us", state.target.vbus_elapsed_us);
	report.number(FIELD(target.input_ma), 0, 3300);
	report.flags(FIELD(target.hardware_limit));
	report.number(FIELD(target.charge_ma), 0, 5000);
	report.flags(FIELD(target.charge_enabled));
	report.choice(FIELD(target.stop), 6);
	report.number(FIELD(target.thermal_limit_c), 0, 120);
}

void inspect(Report& report, const expander::StateData& state) {
	report.begin("expander");
	driver(report, state, state.status);
	report.shown(FIELD(raw_input));
	report.shown(FIELD(raw_output));
	report.shown(FIELD(raw_polarity));
	report.shown(FIELD(raw_configuration));
	report.flag_bytes<11>(FIELD(input));
	report.flag_bytes<8>(FIELD(output));
	report.flag_bytes<8>(FIELD(polarity));
	report.flag_bytes<8>(FIELD(configuration));
}

void inspect(Report& report, const fusb::StateData& state) {
	report.begin("usb_c");
	driver(report, state, state.status);
	report.shown(FIELD(raw_identity));
	report.shown(FIELD(raw_status));
	report.shown(FIELD(raw_status1));
	report.shown(FIELD(raw_type));
	report.shown(FIELD(raw_control1));
	report.number(FIELD(identity.version), 0, 15);
	report.number(FIELD(identity.revision), 0, 15);
	report.flags(FIELD(port.attached));
	report.flags(FIELD(port.vbus));
	report.number(FIELD(port.orientation), 0, 3);
	report.number(FIELD(port.advertised_ma), 0, 3000);
	report.flags(FIELD(port.default_current));
	report.choice(FIELD(role), 9);
	report.flags(FIELD(control.enabled));
	report.number(FIELD(control.debounce_ms), 0, 1000);
	report.number(FIELD(cc_limit_ma), 0, 3000);
	report.flags(FIELD(remedy));
}

void inspect(Report& report, const wifi::StateData& state) {
	report.begin("wifi");
	report.choice(FIELD(status), 3);
	report.flags("switch_known", state.slider_sampled);
	report.flags("wanted", state.slider_enabled);
	report.flags(FIELD(reported_known));
	report.flags("pending_apply", state.set_pending);
	report.flags("reported.enabled", state.last_reported_config.enabled);
	report.text("reported.ssid", state.last_reported_config.ssid);
	report.text("reported.password", state.last_reported_config.password);
	report.number("reported.channel", state.last_reported_config.channel, 0,
				  14);
	report.time(FIELD(sent_us));
	report.time(FIELD(reported_us));
	report.shown(FIELD(sent));
	report.shown(FIELD(mismatches));
	report.shown(FIELD(last_error));
	report.time(FIELD(error_us));
}

void inspect(Report& report, const adsbee::timesync::StateData& state) {
	report.begin("adsbee_clock");
	report.flags("set", state.clock_set_from_gnss);
	report.flags("running", state.burst_running);
	report.time("synced_us", state.last_sync_us);
	report.shown("correction_us", state.last_correction_us);
	report.number(FIELD(rtt_us), 0, 10'000'000);
	report.shown(FIELD(pps));
	report.number("answered", state.burst_replies, 0, 255);
	report.shown(FIELD(last_error));
	report.time(FIELD(error_us));
}
}

void require_valid(const Inspection& inspection) {
	if (!inspection.first_invalid_field.empty()) {
		throw Error(
			std::format("The device sent data this utility cannot read: {}",
						inspection.first_invalid_field));
	}
}

Inspection inspect(const Hello& hello) {
	Report report(0);
	inspect(report, hello);
	return report.result;
}

static void inspect(Report& report, const TrafficListing& listing) {
	report.begin("listed");
	report.shown("total_targets", listing.total_targets);
	for (size_t i = 0; i < listing.nearest_targets.size(); i++) {
		inspect(report, listing.nearest_targets[i].target, i, "listed");
		report.past("past", listing.nearest_targets[i].past);
	}
}

Inspection inspect(const TrafficListing& listing) {
	Report report(INT64_MAX);
	inspect(report, listing);
	return std::move(report.result);
}

static void inspect(Report& report, const States& states, Component component) {
	switch (component) {
		case Component::adsbee:
			inspect(report, states.adsbee);
			break;
		case Component::traffic: {
			size_t present = 0;
			for (size_t slot = 0; slot < states.traffic.size(); slot++) {
				if (states.traffic[slot].last_seen_us) {
					present++;
					inspect(report, states.traffic[slot], slot);
				}
			}
			report.begin("traffic");
			report.shown("targets", present);
			break;
		}
		case Component::barometer:
			inspect(report, states.barometer);
			break;
		case Component::battery:
			inspect(report, states.battery);
			break;
		case Component::charger:
			inspect(report, states.charger);
			break;
		case Component::expander:
			inspect(report, states.expander);
			break;
		case Component::usb_c:
			inspect(report, states.usb_c);
			break;
		case Component::wifi:
			inspect(report, states.wifi);
			break;
		case Component::adsbee_clock:
			inspect(report, states.adsbee_clock);
			break;
	}
}

Inspection inspect(const DeviceState& picture, int64_t latest_timestamp_us) {
	Report report(latest_timestamp_us);
	inspect(report, picture.hello);
	for (const Component component : picture.components) {
		inspect(report, picture.states, component);
	}
	inspect(report, picture.traffic_listing);
	return std::move(report.result);
}
}

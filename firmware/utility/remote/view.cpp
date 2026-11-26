#include "view.hpp"

#include <algorithm>
#include <chrono>
#include <deque>
#include <format>
#include <memory>
#include <random>
#include <string_view>

#include "conflicts.hpp"
#include "device/inspect.hpp"
#include "display/display.hpp"
#include "display/display_mapping.hpp"
#include "display/render_study.h"
#include "display_settings.hpp"
#include "licenses.hpp"
#include "radar_publication.hpp"
#include "settings/settings_model.hpp"
#include "settings_edits.hpp"
#include "zones.hpp"

namespace remote::view {
namespace {
using namespace std::chrono_literals;

constexpr auto kMutedFor = 5min;
constexpr size_t kAskedKept = 16;
const char* name(conflicts::Level level) {
	switch (level) {
		case conflicts::Level::other:
			return "other";
		case conflicts::Level::proximate:
			return "proximate";
		case conflicts::Level::advisory:
			return "advisory";
		case conflicts::Level::alert:
			return "alert";
	}
	return "unknown";
}

struct Silenced {
	conflicts::Id target;
	std::chrono::steady_clock::time_point until;
};

struct DeviceDisplayPreferences {
	display::Settings settings;
	bool device_accepts_settings = false;
	SettingsEdits edits;
	uint16_t altitude_filter_ft = 3000;
	uint32_t own_address = display::NO_ADDRESS;
	std::string own_callsign;
	bool zone_automatic = true;
	std::string zone;
	std::vector<Silenced> silenced;
};

struct State {
	DeviceDisplayPreferences preferences;
	device::DeviceState picture;
	display::RadarGenerations radar;
	uint64_t connection_id = 0;
	// The settings asked of the device and not yet answered: those of this
	// display first, then those of the device.
	enum class Awaited { none, display, device } awaited = Awaited::none;
	std::string serial, display_device_id;
	std::chrono::sys_days day;
	// What the user asked of the display last, the newest at the end.
	std::deque<std::string> asked;
	// The zone the last position with a fix lay in.
	std::string detected_zone;
	bool starting = true;
	uint32_t greeting = std::random_device{}();
	std::vector<display::Target> targets =
		std::vector<display::Target>(usb::layout::kTargets);
	std::vector<conflicts::Id> previous_ring_members;
	std::vector<display::mapping::TargetDetails> target_details;
	std::unique_ptr<Conflicts> conflicts;
	conflicts::StateData conflict_state;

	std::vector<conflicts::Band> bands = std::vector<conflicts::Band>(
		conflicts::kGuided * conflicts::band_capacity);
	std::string explained;
};

State& state() {
	static State instance;
	return instance;
}

bool study_radar() {
	if constexpr (std::string_view{AIRGREP_DISPLAY} == "800x480-touch") {
		return (airgrep_render_study_flags & RENDER_STUDY_GENERATIONS) != 0;
	}
	return false;
}

void changed_picture(const device::DeviceState& before,
					 const device::DeviceState& after) {
	auto& s = state();
	const bool fix = after.states.adsbee
						 .gnss_at(device::end_of_uptime_second_us(after.hello))
						 .fix != adsbee::Fix::none;
	if (fix &&
		!radar::same_traffic(before.traffic_listing, after.traffic_listing,
							 s.preferences.settings.tracks != 0)) {
		s.radar.bump(s.radar.traffic);
	}
	const bool bands = s.preferences.settings.conflict_indicator &&
					   std::ranges::any_of(s.conflict_state.bands,
										   [](const conflicts::Bands& b) {
											   return !b.intervals().empty();
										   });
	if (!radar::same_navigation(
			before, after,
			bands || !after.traffic_listing.nearest_targets.empty())) {
		s.radar.bump(s.radar.gnss);
	}
}

void changed_conflicts(const conflicts::StateData& before,
					   const conflicts::StateData& after) {
	if (!radar::same_conflicts(
			before, after,
			state().preferences.settings.conflict_indicator != 0)) {
		state().radar.bump(state().radar.daidalus);
	}
}

bool poll_conflicts(bool inputs_changed) {
	auto& s = state();
	if (!s.conflicts || s.starting) {
		return false;
	}
	if (inputs_changed) {
		const conflicts::ConflictInput sample{
			s.picture.states.adsbee, s.picture.traffic_listing.nearest_targets,
			s.picture.states.barometer,
			device::end_of_uptime_second_us(s.picture.hello),
			s.preferences.own_address};
		s.conflicts->alert(sample, s.preferences.settings.conflict_indicator);
	}
	return s.conflicts->take_latest(
		s.conflict_state, study_radar() ? changed_conflicts : nullptr);
}

std::optional<uint8_t> busiest_core(const usb::protocol::Hello& hello) {
	std::optional<uint8_t> busiest;
	for (uint8_t core : {hello.cpu_core0, hello.cpu_core1}) {
		if (core != AIRGREP_CPU_UNAVAILABLE && (!busiest || core > *busiest)) {
			busiest = core;
		}
	}
	return busiest;
}

const char* name(conflicts::BandRegion region) {
	switch (region) {
		case conflicts::BandRegion::preventive:
			return "preventive";
		case conflicts::BandRegion::corrective:
			return "corrective";
		case conflicts::BandRegion::warning:
			return "warning";
		case conflicts::BandRegion::recovery:
			return "recovery";
	}
	return "unknown";
}

// Against the own track, altitude and ground speed, as the display draws
// them.
void explain(std::string& text, const char* of,
			 std::span<const conflicts::Band> bands) {
	for (const conflicts::Band& band : bands) {
		text += std::format("{} band {:.0f} to {:.0f}  {}\n", of, band.from,
							band.to, name(band.region));
	}
}

const char* list_status(const display::mapping::TargetDetails& target) {
	if (target.exclusion_reason) {
		using enum display::mapping::ExclusionReason;
		switch (*target.exclusion_reason) {
			case no_report:
				return "empty report";
			case own_ship:
				return "hidden, own ship";
			case no_position:
				return "hidden, no target position";
			case altitude_filter:
				return "hidden by the altitude filter";
			case list_full:
				return "omitted, target list full";
		}
	}
	switch (target.radar_position) {
		case display::RadarPosition::unknown:
			return "listed, radar position unknown";
		case display::RadarPosition::inside_ring:
			return "listed, inside the ring";
		case display::RadarPosition::beyond_ring:
			return target.show_beyond_ring_marker
					   ? "listed, beyond the ring, marked"
					   : "listed, beyond the ring, unmarked";
	}
	return "unknown";
}

std::string explain(
	const display::mapping::ViewInputs& sources,
	const display::ViewState& model,
	std::span<const display::mapping::TargetDetails> target_details) {
	const display::mapping::Preferences& chosen = sources.preferences;
	const adsbee::Gnss& own = sources.adsbee.gnss_at(sources.monotonic_now_us);
	std::string text = std::format(
		"== Display\nclock {:02}:{:02} {}\nrange {} NM  altitude filter {} ft  "
		"tracks {}  own address {}\nown ship: {}  speed {:.1f} kt  track "
		"{:.1f}\nlisted {} of {} reports, {} inside the ring\nconflict "
		"indicator {}  bands: track {}  altitude {}  speed {}\n",
		sources.clock.hour, sources.clock.minute, sources.clock.time_zone,
		chosen.radar_range_nm, chosen.altitude_filter_ft,
		chosen.track_history ? "on" : "off",
		chosen.own_address == display::NO_ADDRESS
			? "none"
			: std::format("{:06X}", chosen.own_address),
		model.fix == display::Fix::none ? "no fix" : "fix", own.speed_kt,
		own.track_deg, model.traffic.size(), sources.traffic.size(),
		model.traffic_inside, model.conflict_indicator ? "on" : "off",
		model.track_bands.size(), model.altitude_bands.size(),
		model.speed_bands.size());
	explain(text, "track", model.track_bands);
	explain(text, "altitude", model.altitude_bands);
	explain(text, "speed", model.speed_bands);
	for (size_t i = 0; i < target_details.size(); ++i) {
		const traffic::Target& report = sources.traffic[i].target;
		const auto& at = target_details[i];
		std::string conflict;
		if (at.conflict) {
			conflict =
				at.conflict->level == conflicts::Level::other
					? ", no conflict"
					: std::format(", {} in {} s", name(at.conflict->level),
								  at.conflict->within_s);
		}
		text += std::format(
			"{:<8} {:06X}  {}{}\n  {}  altitude {}  speed {}\n",
			report.callsign[0] ? report.callsign : "-", report.address,
			list_status(at), conflict,
			at.position ? std::format("{:.2f} NM at {:05.1f} true",
									  at.position->distance_nm,
									  at.position->true_bearing_deg)
						: "not placed",
			at.above_ft ? std::format("{:+} ft", *at.above_ft) : "unknown",
			report.speed_valid ? std::format("{} kt", report.speed_kt)
							   : "unknown");
	}
	return text;
}

void refresh() {
	const device::DeviceState& picture = state().picture;
	auto now = std::chrono::steady_clock::now();
	if (std::erase_if(state().preferences.silenced, [&](const Silenced& entry) {
			return entry.until <= now;
		})) {
		state().radar.bump(state().radar.traffic);
	}
	std::vector<conflicts::Id> muted;
	muted.reserve(state().preferences.silenced.size());
	for (const Silenced& entry : state().preferences.silenced) {
		muted.push_back(entry.target);
	}
	const int64_t now_us = device::end_of_uptime_second_us(picture.hello);
	const adsbee::Gnss& gnss = picture.states.adsbee.gnss_at(now_us);
	const int64_t utc_s =
		state().day.time_since_epoch().count() * int64_t{86400} +
		int64_t{gnss.utc_hour} * 3600 + int64_t{gnss.utc_minute} * 60 +
		gnss.utc_second;
	if (gnss.fix != adsbee::Fix::none) {
		const char* zone = zones::at(gnss.latitude, gnss.longitude);
		if (zone[0]) {
			state().detected_zone = zone;
		}
	}
	const DeviceDisplayPreferences& chosen = state().preferences;
	const char* zone_in_use = chosen.zone_automatic
								  ? state().detected_zone.c_str()
								  : chosen.zone.c_str();
	display::mapping::ViewInputs sources{
		.adsbee = picture.states.adsbee,
		.traffic = picture.traffic_listing.nearest_targets,
		.previous_inside = state().previous_ring_members,
		.barometer = picture.states.barometer,
		.battery = picture.states.battery,
		.charger = picture.states.charger,
		.expander = picture.states.expander,
		.wifi = picture.states.wifi,
		.monotonic_now_us = now_us,
		.health = picture.hello.health,
		.busiest_core_percent = busiest_core(picture.hello),
		.mode = static_cast<power_mode_t>(picture.hello.mode),
		.starting = state().starting,
		.device_name = picture.hello.board,
		.version = picture.hello.firmware_version,
		.device_id = state().display_device_id.c_str(),
		.greeting = state().greeting,
		.clock = gnss.utc_known ? zones::local(zone_in_use, utc_s)
								: display::mapping::Clock{0, 0, "UTC"},
		.zones = {.automatic = chosen.zone_automatic,
				  .in_use = zone_in_use,
				  .detected = state().detected_zone.c_str(),
				  .listed = zones::listed(utc_s)},
		.preferences =
			{.language = state().preferences.settings.language,
			 .dark_theme =
				 state().preferences.settings.theme == display::Theme::dark,
			 .track_history = state().preferences.settings.tracks != 0,
			 .simple_symbols = state().preferences.settings.symbol_style ==
							   display::SymbolStyle::simple,
			 .conflict_indicator =
				 state().preferences.settings.conflict_indicator != 0,
			 .radar_range_nm = display::RANGES_NM
				 [state().preferences.settings.radar_range_index],
			 .altitude_filter_ft = state().preferences.altitude_filter_ft,
			 .own_address = state().preferences.own_address,
			 .own_callsign = state().preferences.own_callsign.c_str(),
			 .muted = muted},
		.licenses = kLicenses};
	sources.conflicts = std::span(state().conflict_state.results)
							.first(state().conflict_state.result_count);
	conflicts::show(state().conflict_state, sources);
	state().target_details.resize(sources.traffic.size());
	display::ViewState model = display::mapping::build_view_state(
		sources, state().targets, state().bands, state().target_details);
	state().explained = explain(sources, model, state().target_details);
	for (const std::string& asked : state().asked) {
		state().explained += "asked " + asked + "\n";
	}
	if (study_radar()) {
		const auto id = [](const auto& target) {
			return std::tie(target.address, target.source);
		};
		if (!std::ranges::equal(state().previous_ring_members,
								model.traffic.first(model.traffic_inside), {},
								id, id)) {
			state().radar.bump(state().radar.traffic);
		}
		model.radar_generations = state().radar;
	}
	state().previous_ring_members.resize(model.traffic_inside);
	display::mapping::inside_ids(model, state().previous_ring_members);
	display::update(model);
	if (model.radar_generations) {
		state().radar.acknowledge();
	}
}

void note(std::string asked) {
	state().asked.push_back(std::move(asked));
	if (state().asked.size() > kAskedKept) {
		state().asked.pop_front();
	}
}

void mark_settings_dirty_and_refresh(SettingsEdits::Field field) {
	state().preferences.edits.changed.set(field);
	if (field == SettingsEdits::indicator) {
		poll_conflicts(true);
	}
	refresh();
}
}

display::Actions actions() {
	return {.set_dark_theme =
				[](bool dark) {
					note(std::format("set_dark_theme {:d}", dark));
					state().preferences.settings.theme =
						dark ? display::Theme::dark : display::Theme::light;
					mark_settings_dirty_and_refresh(SettingsEdits::theme);
				},
			.set_track_history =
				[](bool on) {
					note(std::format("set_track_history {:d}", on));
					state().preferences.settings.tracks = on;
					mark_settings_dirty_and_refresh(SettingsEdits::tracks);
				},
			.set_simple_symbols = !display::Settings::supports_symbol_style ? nullptr : +[](bool simple) {
					note(std::format("set_simple_symbols {:d}", simple));
					state().preferences.settings.symbol_style =
						simple ? display::SymbolStyle::simple
							   : display::SymbolStyle::planes;
					mark_settings_dirty_and_refresh(SettingsEdits::symbols);
				},
			.set_conflict_indicator =
				[](bool on) {
					note(std::format("set_conflict_indicator {:d}", on));
					state().preferences.settings.conflict_indicator = on;
					mark_settings_dirty_and_refresh(SettingsEdits::indicator);
				},
			.set_radar_range =
				[](uint8_t range_nm) {
					note(std::format("set_radar_range {}", range_nm));
					auto step = display::radar_range_index(range_nm);
					if (step) {
						state().preferences.settings.radar_range_index = *step;
						mark_settings_dirty_and_refresh(SettingsEdits::radar_range);
					}
				},
			.set_altitude_filter =
				[](uint16_t feet) {
					note(std::format("set_altitude_filter {}", feet));
					state().preferences.altitude_filter_ft = feet;
					refresh();
				},
			.set_time_zone_automatic =
				[](bool on) {
					note(std::format("set_time_zone_automatic {:d}", on));
					state().preferences.zone_automatic = on;
					refresh();
				},
			.set_time_zone =
				[](const char* zone) {
					note(std::format("set_time_zone {}", zone[0] ? zone : "UTC"));
					state().preferences.zone = zone;
					state().preferences.zone_automatic = false;
					refresh();
				},
			.set_muted =
				[](display::TargetId id, bool muted) {
					auto [address, source] = id;
					note(std::format("{} {:06X}", muted ? "mute" : "unmute",
									 address));
					source = mute_source(source);
					const auto matches = [&](const Silenced& entry) {
						return entry.target.address == address &&
							   entry.target.source == source;
					};
					auto& silenced = state().preferences.silenced;
					const bool was_muted = std::erase_if(silenced, matches) != 0;
					if (was_muted != muted) {
						state().radar.bump(state().radar.traffic);
					}
					if (muted) {
						silenced.push_back(
							{{address, source},
							 std::chrono::steady_clock::now() + kMutedFor});
					}
					refresh();
				},
			.set_own_ship =
				[](uint32_t address) {
					note(address == display::NO_ADDRESS
							 ? "set_own_ship none"
							 : std::format("set_own_ship {:06X}", address));
					state().preferences.own_address = address;
					poll_conflicts(true);
					refresh();
				},
			.set_own_callsign =
				[](const char* callsign) {
					note(std::format("set_own_callsign {}", callsign));
					state().preferences.own_callsign = callsign;
					refresh();
				}};
}

void follow(Connection& connection, const std::string& serial) {
	if (const auto day = connection.day(); day != state().day) {
		state().day = day;
		if (state().connection_id == connection.id()) {
			refresh();
		}
	}
	const Connection::Status status = connection.status();
	if (status.phase == Connection::Phase::ended) {
		if (connection.id() == state().connection_id) {
			state().conflicts.reset();
		}
		return;
	}
	if (status.phase != Connection::Phase::connected ||
		!status.supports_component_reads) {
		return;
	}
	if (serial != state().serial) {
		state().preferences = {};
		state().serial = serial;
		state().detected_zone.clear();
		state().asked.clear();
		state().display_device_id = "airgrep-" + serial;
	}
	bool conflicts_restarted = false;
	if (connection.id() != state().connection_id) {
		state().radar.resync = true;
		state().connection_id = connection.id();
		state().picture = {};
		state().picture.hello = status.hello;
		state().starting = true;
		state().conflicts = std::make_unique<Conflicts>();
		state().conflict_state = {};
		state().preferences.device_accepts_settings = false;
		state().preferences.edits.changed.reset();
		state().previous_ring_members.clear();
		connection.read_settings(display::kSettingsKey);
		state().awaited = State::Awaited::display;
		refresh();
	} else if (!state().conflicts) {
		state().conflicts = std::make_unique<Conflicts>();
		conflicts_restarted = true;
	}
	const auto answered = state().awaited == State::Awaited::none
							  ? std::nullopt
							  : connection.settings_read();
	if (answered && state().awaited == State::Awaited::device) {
		state().awaited = State::Awaited::none;
		// What the user stored on the device holds until this display changes
		// it.
		settings::Settings read{};
		if (settings::migrate(answered->data(), answered->size(), read)) {
			state().preferences.altitude_filter_ft = read.altitude_filter_ft;
			state().preferences.own_address =
				read.own_address == SETTINGS_NO_ADDRESS ? display::NO_ADDRESS
														: read.own_address;
			state().preferences.own_callsign = read.own_callsign;
			state().preferences.zone_automatic = read.tz_detect != 0;
			state().preferences.zone = read.zone;
			poll_conflicts(true);
			refresh();
		}
	} else if (const auto& bytes = answered) {
		state().awaited = State::Awaited::device;
		connection.read_settings(SETTINGS_NVS_KEY);
		// Bytes of another size or with a value this display does not take
		// are those of other firmware: it keeps them, the display its own.
		display::Settings read;
		if (bytes->size() == sizeof read) {
			std::memcpy(&read, bytes->data(), sizeof read);
			if (valid(read)) {
				state().preferences.settings = state().preferences.edits.merge(
					read, state().preferences.settings);
				state().preferences.device_accepts_settings = true;
				poll_conflicts(true);
				refresh();
			}
		}
	}
	if (state().preferences.device_accepts_settings &&
		state().preferences.edits.changed.any()) {
		const auto* bytes =
			reinterpret_cast<const uint8_t*>(&state().preferences.settings);
		connection.write_settings(
			display::kSettingsKey,
			{bytes, bytes + sizeof state().preferences.settings});
		state().preferences.edits.changed.reset();
	}
	const bool new_picture = connection.take_new_picture(
		state().picture, study_radar() ? changed_picture : nullptr);
	if (new_picture) {
		state().starting = false;
	}
	if (poll_conflicts(new_picture || conflicts_restarted) || new_picture) {
		refresh();
	}
}

void unfollow() { state().conflicts.reset(); }

bool muted() { return state().picture.states.expander.input.mute; }

const std::string& explained() { return state().explained; }
}

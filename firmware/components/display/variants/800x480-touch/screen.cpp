#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "airgrep_ui.h"
#include "conflicts/conflicts.hpp"
#include "display/display.hpp"
#include "greetings.hpp"
#include "variant.hpp"
#include "widgets/entry_field/entry_field.h"
#include "widgets/keyboard/keyboard.h"
#include "widgets/radar/radar.h"
#include "widgets/traffic_table/traffic_table.h"

namespace display {
namespace {
constexpr uint32_t BLINK_HALF_MS = 1000;
constexpr uint32_t IDLE_RETURN_MS = 10000;
constexpr uint32_t IDLE_RETURN_READING_MS = 30000;
// Travel in pixels before a touch stops being a tap, 1.9 mm on this panel.
// LVGL's 10 is passed without intent in turbulence.
constexpr uint8_t SCROLL_LOCK = 16;

enum Page : int32_t {
	HOME,
	SYSTEM,
	WIFI_BATTERY,
	LICENSES,
	LICENSE,
	ICAO_ADDRESS,
	CALLSIGN,
	TIME_ZONE,
	ZONES,
	ALTITUDE
};
struct PageSettings {
	Page parent;
	uint32_t idle_return_ms;
};

constexpr PageSettings page_settings(Page page) {
	switch (page) {
		case HOME:
		case SYSTEM:
		case WIFI_BATTERY:
			return {HOME, IDLE_RETURN_MS};
		case LICENSE:
			return {LICENSES, IDLE_RETURN_READING_MS};
		case ZONES:
			return {TIME_ZONE, IDLE_RETURN_READING_MS};
		case LICENSES:
		case ICAO_ADDRESS:
		case CALLSIGN:
		case TIME_ZONE:
		case ALTITUDE:
			return {SYSTEM, IDLE_RETURN_READING_MS};
	}
	return {HOME, IDLE_RETURN_MS};
}

Actions actions;
lv_obj_t* radar;
lv_obj_t* table;
lv_obj_t* wifi_qr;
lv_obj_t* transient_mode_screen;
Mode shown_mode = Mode::on;
// The least the fill of the charger screen shows, so that 0 % is not nothing.
constexpr int32_t CHARGER_FILL_MIN = 8;
std::span<const License> licenses;

void pulse(lv_subject_t* transition, Page page) {
	lv_subject_set_int(transition, page);
	lv_subject_set_int(transition, HOME);
}

void navigate(Page page) {
	const Page front = static_cast<Page>(lv_subject_get_int(subject_page));
	if (front == page) {
		return;
	}
	if (page_settings(page).parent != front) {
		for (Page open = front; open != HOME;
			 open = page_settings(open).parent) {
			pulse(subject_page_closing, open);
		}
	}
	if (page != HOME) {
		lv_subject_set_int(subject_page_closing, HOME);
		pulse(subject_page_opening, page);
	}
	lv_subject_set_int(subject_page, page);
}

lv_obj_t* find(const char* page, const char* name) {
	return lv_obj_find_by_name(lv_obj_find_by_name(home, page), name);
}

// LVGL 9.6.0 lets one fast move of the finger carry a sheet past its end,
// see elastic_diff() in lv_indev_scroll.c. This takes it back.
void keep_inside(lv_event_t* event) {
	lv_obj_t* sheet = lv_event_get_current_target_obj(event);
	const int32_t over = std::min<int32_t>(lv_obj_get_scroll_bottom(sheet), 0);
	if (over < 0) {
		lv_obj_scroll_by(sheet, 0, -over, LV_ANIM_OFF);
	}
}

void license_chosen(lv_event_t* event) {
	const License& license =
		licenses[reinterpret_cast<uintptr_t>(lv_event_get_user_data(event))];
	lv_label_set_text(find("page_license", "bar_title"), license.component);
	lv_label_set_text_static(find("page_license", "license_text"),
							 license.text);
	lv_obj_scroll_to_y(find("page_license", "sheet"), 0, LV_ANIM_OFF);
	navigate(LICENSE);
}

void show_licenses(const ViewState& model) {
	lv_obj_t* rows =
		lv_obj_find_by_name(find("page_licenses", "license_list"), "rows");
	if (lv_obj_get_child_count(rows) == model.licenses.size()) {
		return;
	}
	licenses = model.licenses;
	lv_obj_clean(rows);
	for (size_t i = 0; i < licenses.size(); i++) {
		lv_obj_t* row =
			license_row_create(rows, licenses[i].component, licenses[i].spdx_id,
							   i + 1 < licenses.size());
		lv_obj_add_event_cb(row, license_chosen, LV_EVENT_CLICKED,
							reinterpret_cast<void*>(i));
	}
}

constexpr std::string_view UTC = "Etc/UTC";
constexpr int32_t ROW = 64;
constexpr int32_t REVEAL_CONTEXT_ROWS = 2;
struct {
	std::span<const Zone> zones;
	std::string in_use, detected, language;
	// Without Etc, whose one zone is the UTC row.
	std::vector<std::string> regions;
	std::string shown_region;
	bool rows_current{};
} choice;

std::string_view region(std::string_view zone) {
	return zone.substr(0, zone.find('/'));
}

std::string zone_display_name(std::string_view name) {
	std::string text(name);
	std::ranges::replace(text, '_', ' ');
	return text;
}

std::string place(std::string_view zone) {
	return zone == UTC ? "UTC"
					   : zone_display_name(zone.substr(zone.rfind('/') + 1));
}

std::string offset(int16_t minutes) {
	char text[16] = "UTC";
	const unsigned away = std::abs(minutes);
	if (away % 60) {
		(void)std::snprintf(text, sizeof text, "UTC%s%u:%02u",
							minutes < 0 ? TRAFFIC_MINUS : "+", away / 60,
							away % 60);
	} else if (away) {
		(void)std::snprintf(text, sizeof text, "UTC%s%u",
							minutes < 0 ? TRAFFIC_MINUS : "+", away / 60);
	}
	return text;
}

void show_check(const lv_obj_t* row, bool chosen) {
	lv_obj_set_style_image_opa(lv_obj_find_by_name(row, "check"),
							   chosen ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
}

void pick(std::string_view zone) {
	if (actions.set_time_zone) {
		actions.set_time_zone(zone == UTC ? "" : std::string(zone).c_str());
	}
	navigate(SYSTEM);
}

lv_obj_t* rows_of(const char* page, const char* list) {
	lv_obj_t* rows = lv_obj_find_by_name(find(page, list), "rows");
	lv_obj_clean(rows);
	return rows;
}

// The zones of the region that is shown, each with its offset from UTC. A
// region holds more rows than LVGL has memory for, America 144. So the list
// is as high as all of them and only the rows in view exist: they move and
// are filled again as the sheet scrolls.
constexpr int32_t ROWS_IN_VIEW = 7;
struct {
	std::vector<const Zone*> zones;
	lv_obj_t* rows[ROWS_IN_VIEW]{};
	std::optional<int32_t> first_bound_zone_index;
} listed;

void fill_zones(bool again) {
	const lv_obj_t* sheet = find("page_zones", "sheet");
	const int32_t count = static_cast<int32_t>(listed.zones.size());
	int32_t first =
		std::clamp<int32_t>(lv_obj_get_scroll_y(sheet) / ROW, 0,
							std::max<int32_t>(count - ROWS_IN_VIEW, 0));
	if (first == listed.first_bound_zone_index && !again) {
		return;
	}
	listed.first_bound_zone_index = first;
	for (int32_t i = 0; i < ROWS_IN_VIEW; i++) {
		lv_obj_t* row = listed.rows[i];
		const int32_t index = first + i;
		lv_obj_set_hidden(row, index >= count);
		if (index >= count) {
			continue;
		}
		const std::string_view name = listed.zones[index]->iana_name;
		lv_obj_set_y(row, index * ROW);
		lv_label_set_text(
			lv_obj_find_by_name(row, "label"),
			zone_display_name(name.substr(choice.shown_region.size() + 1))
				.c_str());
		lv_label_set_text(lv_obj_find_by_name(row, "note"),
						  offset(listed.zones[index]->offset_min).c_str());
		lv_obj_set_style_border_width(lv_obj_find_by_name(row, "line"),
									  index + 1 < count, 0);
		show_check(row, name == choice.in_use);
		lv_obj_set_user_data(row, const_cast<Zone*>(listed.zones[index]));
	}
}

void show_zones(bool scroll_to_selection) {
	lv_obj_t* sheet = find("page_zones", "sheet");
	lv_obj_t* rows =
		lv_obj_find_by_name(find("page_zones", "zone_list"), "rows");
	if (!listed.rows[0]) {
		lv_obj_set_layout(rows, LV_LAYOUT_NONE);
		for (lv_obj_t*& row : listed.rows) {
			row = choice_row_create(rows, "", "", true);
			lv_obj_add_event_cb(
				row,
				[](lv_event_t* event) {
					pick(static_cast<const Zone*>(
							 lv_obj_get_user_data(
								 lv_event_get_current_target_obj(event)))
							 ->iana_name);
				},
				LV_EVENT_CLICKED, nullptr);
		}
		lv_obj_add_event_cb(
			sheet, [](lv_event_t*) { fill_zones(false); }, LV_EVENT_SCROLL,
			nullptr);
	}
	listed.zones.clear();
	int32_t chosen = 0;
	for (const Zone& zone : choice.zones) {
		if (region(zone.iana_name) == choice.shown_region) {
			if (zone.iana_name == choice.in_use) {
				chosen = static_cast<int32_t>(listed.zones.size());
			}
			listed.zones.push_back(&zone);
		}
	}
	lv_obj_set_height(rows, static_cast<int32_t>(listed.zones.size()) * ROW);
	if (scroll_to_selection) {
		lv_obj_update_layout(sheet);
		lv_obj_scroll_to_y(sheet, 0, LV_ANIM_OFF);
		const int32_t wanted =
			std::max<int32_t>(chosen - REVEAL_CONTEXT_ROWS, 0) * ROW;
		lv_obj_scroll_to_y(sheet,
						   std::min(wanted, lv_obj_get_scroll_bottom(sheet)),
						   LV_ANIM_OFF);
	}
	fill_zones(true);
}

void region_chosen(lv_event_t* event) {
	choice.shown_region = choice.regions[reinterpret_cast<uintptr_t>(
		lv_event_get_user_data(event))];
	lv_label_set_text(find("page_zones", "bar_title"),
					  choice.shown_region.c_str());
	show_zones(true);
	navigate(ZONES);
}

// UTC and the detected zone, then the regions. A region that holds the
// chosen zone names it, unless a row above already carries the checkmark.
void show_regions() {
	lv_obj_t* quick = rows_of("page_time_zone", "quick_list");
	lv_obj_t* row = choice_row_create(quick, "UTC", "", true);
	show_check(row, choice.in_use == UTC);
	lv_obj_add_event_cb(
		row, [](lv_event_t*) { pick(""); }, LV_EVENT_CLICKED, nullptr);
	const bool found = !choice.detected.empty();
	const bool at_found = found && choice.detected == choice.in_use;
	row =
		choice_row_create(quick, lv_tr("detected"),
						  found ? place(choice.detected).c_str() : "–", false);
	show_check(row, at_found && choice.in_use != UTC);
	lv_obj_set_clickable(row, found);
	lv_obj_add_event_cb(
		row, [](lv_event_t*) { pick(choice.detected); }, LV_EVENT_CLICKED,
		nullptr);

	lv_obj_t* regions = rows_of("page_time_zone", "region_list");
	for (size_t i = 0; i < choice.regions.size(); i++) {
		const bool holds = !at_found && choice.in_use != UTC &&
						   region(choice.in_use) == choice.regions[i];
		row = license_row_create(regions, choice.regions[i].c_str(),
								 holds ? place(choice.in_use).c_str() : "",
								 i + 1 < choice.regions.size());
		lv_obj_add_event_cb(row, region_chosen, LV_EVENT_CLICKED,
							reinterpret_cast<void*>(i));
	}
}

void draw_choice() {
	if (!choice.rows_current) {
		choice.rows_current = true;
		show_regions();
		if (!choice.shown_region.empty()) {
			show_zones(false);
		}
	}
}

bool same(std::span<const Zone> a, std::span<const Zone> b) {
	return a.data() == b.data() && a.size() == b.size();
}

void show_time_zone(const ViewState& model) {
	const std::string_view active =
		*model.zone ? std::string_view(model.zone) : UTC;
	lv_subject_copy_string(subject_zone, place(active).c_str());
	lv_obj_set_clickable(find("page_system", "time_zone"),
						 !model.time_zone_automatic);
	const bool known = same(choice.zones, model.zones);
	if (known && choice.in_use == active &&
		choice.detected == model.zone_detected &&
		choice.language == model.language) {
		return;
	}
	choice.in_use = active;
	choice.detected = model.zone_detected;
	choice.language = model.language;
	if (!known) {
		choice.zones = model.zones;
		choice.regions.clear();
		for (const Zone& zone : model.zones) {
			const std::string_view name = region(zone.iana_name);
			if (name != region(UTC) &&
				(choice.regions.empty() || choice.regions.back() != name)) {
				choice.regions.emplace_back(name);
			}
		}
	}
	choice.rows_current = false;
	if (lv_subject_get_int(subject_page) >= TIME_ZONE) {
		draw_choice();
	}
}

// A page with a field and keys. It edits a draft: until the first key the
// field shows what is stored, and only Save hands the draft to the logic.
struct Entry {
	Page page;
	const char* page_name;
	const char* row_name;
	uint32_t places;
	uint32_t min_length;
	bool empty_clears_setting = false;
	bool decimal_without_leading_zero = false;
	void (*save)(const char* draft);
	lv_obj_t* field = nullptr;
	lv_obj_t* keys = nullptr;
	char stored[9]{};
	char draft[9]{};
	bool typed = false;
};
void save_icao_address(const char* draft) {
	if (actions.set_own_ship) {
		actions.set_own_ship(
			draft[0] ? static_cast<uint32_t>(std::strtoul(draft, nullptr, 16))
					 : NO_ADDRESS);
	}
}
void save_callsign(const char* draft) {
	if (actions.set_own_callsign) {
		actions.set_own_callsign(draft);
	}
}
void save_altitude_filter(const char* draft) {
	if (actions.set_altitude_filter) {
		actions.set_altitude_filter(
			static_cast<uint16_t>(std::strtoul(draft, nullptr, 10)));
	}
}
Entry icao_address{.page = ICAO_ADDRESS,
				   .page_name = "page_icao",
				   .row_name = "icao_address",
				   .places = 6,
				   .min_length = 6,
				   .empty_clears_setting = true,
				   .save = save_icao_address};
Entry callsign{.page = CALLSIGN,
			   .page_name = "page_callsign",
			   .row_name = "callsign",
			   .places = 8,
			   .min_length = 1,
			   .empty_clears_setting = true,
			   .save = save_callsign};
constexpr uint32_t decimal_digits(uint32_t number) noexcept {
	return number < 10 ? 1 : 1 + decimal_digits(number / 10);
}
Entry altitude_filter{.page = ALTITUDE,
					  .page_name = "page_altitude",
					  .row_name = "altitude_filter",
					  .places = decimal_digits(ALTITUDE_FILTER_MAX_FT),
					  .min_length = 1,
					  .decimal_without_leading_zero = true,
					  .save = save_altitude_filter};
Entry* const ENTRIES[] = {&icao_address, &callsign, &altitude_filter};

void show_entry(const Entry& entry) {
	const size_t length = std::strlen(entry.draft);
	entry_field_set(entry.field, entry.places,
					entry.typed ? entry.draft : entry.stored, !entry.typed);
	keyboard_set_save_active(entry.keys,
							 entry.typed &&
								 std::strcmp(entry.draft, entry.stored) != 0 &&
								 (length >= entry.min_length ||
								  (length == 0 && entry.empty_clears_setting)));
}

void open_entry(lv_event_t* event) {
	Entry& entry = *static_cast<Entry*>(lv_event_get_user_data(event));
	entry.typed = false;
	show_entry(entry);
	navigate(entry.page);
}

void key_tapped(lv_obj_t* keys, int key) {
	auto found = std::ranges::find(ENTRIES, keys, &Entry::keys);
	if (found == std::end(ENTRIES)) {
		return;
	}
	Entry& entry = **found;
	if (key == KEYBOARD_KEY_SAVE) {
		entry.save(entry.draft);
		navigate(SYSTEM);
		return;
	}
	if (!entry.typed) {
		entry.typed = true;
		entry.draft[0] = '\0';
	}
	size_t length = std::strlen(entry.draft);
	if (key >= 0 && entry.decimal_without_leading_zero &&
		std::strcmp(entry.draft, "0") == 0) {
		length = 0;
	}
	if (key >= 0 && length < entry.places) {
		entry.draft[length] = static_cast<char>(key);
		entry.draft[length + 1] = '\0';
	} else if (key == KEYBOARD_KEY_DELETE && length > 0) {
		entry.draft[length - 1] = '\0';
	}
	show_entry(entry);
}

void show_own_ship(const ViewState& model) {
	icao_address.stored[0] = '\0';
	if (model.own_address != NO_ADDRESS) {
		(void)std::snprintf(
			icao_address.stored, sizeof(icao_address.stored), "%06X",
			static_cast<unsigned>(model.own_address & 0xFFFFFFU));
	}
	(void)std::snprintf(callsign.stored, sizeof(callsign.stored), "%s",
						model.own_callsign);
	lv_subject_copy_string(subject_icao, icao_address.stored[0]
											 ? icao_address.stored
											 : lv_tr("not_set"));
	lv_subject_copy_string(subject_callsign, callsign.stored[0]
												 ? callsign.stored
												 : lv_tr("not_set"));
	show_entry(icao_address);
	show_entry(callsign);
}

void show_filter(const ViewState& model) {
	(void)std::snprintf(altitude_filter.stored, sizeof(altitude_filter.stored),
						"%u", model.altitude_filter_ft);
	lv_subject_snprintf(subject_altitude_filter, "%s ft",
						altitude_filter.stored);
	show_entry(altitude_filter);
}

struct Wording {
	Cause cause;
	const char* label_tag;
	const char* state_tag;
};

constexpr Wording health_messages_by_priority[] = {
	{Cause::i2c, "i2c", "unresponsive"},
	{Cause::adsbee, "adsbee", "unresponsive"},
	{Cause::gps_receiver, "gps_receiver", "unresponsive"},
	{Cause::antenna_short, "gps_antenna", "antenna_short"},
	{Cause::charger_overvoltage, "charger", "overvoltage"},
	{Cause::charger_overheated, "charger", "overheated"},
	{Cause::battery_overheated, "battery", "overheated"},
	{Cause::antenna_open, "gps_antenna", "antenna_disconnected"},
	{Cause::backup_battery, "gps_receiver", "backup_battery"},
	{Cause::barometer, "barometer", "unresponsive"},
	{Cause::battery_gauge, "battery", "unresponsive"},
	{Cause::usb_c, "usb_c", "unresponsive"},
	{Cause::battery_low, "battery", "battery_low"},
	{Cause::battery_cold, "battery", "too_cold"},
	{Cause::memory, "memory", "oom"},
	{Cause::cpu, "cpu", nullptr},
};

bool case_insensitive_less(const char* left, const char* right) {
	for (; *left && *right; left++, right++) {
		const int difference = std::tolower(static_cast<unsigned char>(*left)) -
							   std::tolower(static_cast<unsigned char>(*right));
		if (difference != 0) {
			return difference < 0;
		}
	}
	return !*left && *right;
}

void show_health(const ViewState& model) {
	static struct {
		bool drawn = false;
		uint16_t causes{};
		std::optional<uint8_t> cpu_percent;
		std::string language;
		std::vector<lv_obj_t*> rows;
	} shown;
	const char* language = lv_translation_get_language();
	const bool cpu = model.causes & bit(Cause::cpu);
	if (shown.drawn && shown.causes == model.causes &&
		shown.language == language &&
		(!cpu || shown.cpu_percent == model.busiest_core_percent)) {
		return;
	}
	shown.drawn = true;
	shown.causes = model.causes;
	shown.cpu_percent = model.busiest_core_percent;
	shown.language = language;

	std::vector<const Wording*> found;
	for (const auto& message : health_messages_by_priority) {
		if ((model.causes & bit(message.cause)) &&
			std::ranges::none_of(found, [&](const Wording* previous) {
				return std::string_view(previous->label_tag) ==
					   message.label_tag;
			})) {
			found.push_back(&message);
		}
	}
	std::ranges::sort(found, [](const Wording* left, const Wording* right) {
		return case_insensitive_less(lv_tr(left->label_tag),
									 lv_tr(right->label_tag));
	});
	lv_obj_t* rows =
		lv_obj_find_by_name(find("page_system", "health_list"), "rows");
	lv_obj_set_hidden(lv_obj_find_by_name(rows, "all_systems"), !found.empty());
	while (shown.rows.size() < found.size()) {
		shown.rows.push_back(cause_row_create(rows, "", "", true));
	}
	for (size_t i = 0; i < shown.rows.size(); i++) {
		lv_obj_t* row = shown.rows[i];
		lv_obj_set_hidden(row, i >= found.size());
		if (i >= found.size()) {
			continue;
		}
		const Wording& wording = *found[i];
		char load[8];
		const char* state = wording.state_tag ? lv_tr(wording.state_tag) : "–";
		if (wording.cause == Cause::cpu && model.busiest_core_percent) {
			(void)std::snprintf(
				load, sizeof(load), "%u %%",
				static_cast<unsigned>(*model.busiest_core_percent));
			state = load;
		}
		const bool failure = level(bit(wording.cause)) == Health::failure;
		lv_obj_t* failed = lv_obj_find_by_name(row, "failure");
		lv_obj_t* caution = lv_obj_find_by_name(row, "caution");
		lv_label_set_text(lv_obj_find_by_name(row, "label"),
						  lv_tr(wording.label_tag));
		lv_label_set_text(failed, state);
		lv_label_set_text(caution, state);
		lv_obj_set_hidden(failed, !failure);
		lv_obj_set_hidden(caution, failure);
		lv_obj_set_style_border_width(lv_obj_find_by_name(row, "line"),
									  i + 1 < found.size(), 0);
	}
}

// The symbol of the header, as subject_gps_symbol counts. A cause that
// explains a missing fix takes the place of the fix.
int32_t gps_symbol(const ViewState& model) {
	if (model.causes & bits({Cause::adsbee, Cause::gps_receiver})) {
		return 5;
	}
	if (model.causes & bit(Cause::antenna_short)) {
		return 3;
	}
	if (model.causes & bit(Cause::antenna_open)) {
		return 4;
	}
	return static_cast<int32_t>(model.fix);
}

void show_language(const ViewState& model) {
	if (std::strcmp(lv_translation_get_language(), model.language) != 0) {
		lv_translation_set_language(model.language);
		keyboard_set_layout(callsign.keys, model.language);
	}
}

// One greeting of the language. One that names the callsign needs a callsign.
// The build has made sure that each fits the screen with any callsign.
std::string greeting(const ViewState& model) {
	const greetings::Language* language = &greetings::LANGUAGES[0];
	for (const greetings::Language& candidate : greetings::LANGUAGES) {
		if (std::strcmp(candidate.code, model.language) == 0) {
			language = &candidate;
		}
	}
	std::string_view placeholder = greetings::PLACEHOLDER;
	auto usable = language->greetings |
				  std::views::filter([&](std::string_view candidate) {
					  return model.own_callsign[0] != '\0' ||
							 !candidate.contains(placeholder);
				  });
	const auto count = static_cast<uint32_t>(std::ranges::distance(usable));
	if (count == 0) {
		return {};
	}
	std::string text = *std::ranges::next(
		usable.begin(),
		static_cast<std::ptrdiff_t>(model.greeting_seed % count));
	if (const size_t at = text.find(placeholder); at != std::string::npos) {
		text.replace(at, placeholder.size(), model.own_callsign);
	}
	return text;
}

void show_startup(const ViewState& model) {
	lv_label_set_text_fmt(lv_obj_find_by_name(transient_mode_screen, "unit"),
						  "%s · %s", model.device_name, model.version);
	lv_label_set_text(lv_obj_find_by_name(transient_mode_screen, "greeting"),
					  greeting(model).c_str());
}

// The fill is as wide as the charge. The percentage in it lies exactly on the
// one below, so the fill's edge is where the colour of the digits changes.
void show_charger(const ViewState& model) {
	const lv_obj_t* well = lv_obj_find_by_name(transient_mode_screen, "well");
	lv_obj_t* fill = lv_obj_find_by_name(well, "fill");
	const lv_obj_t* percent = lv_obj_find_by_name(well, "percent");
	lv_label_set_text_fmt(lv_obj_find_by_name(well, "number"), "%u",
						  model.battery_percent);
	lv_label_set_text_fmt(lv_obj_find_by_name(well, "number_on"), "%u",
						  model.battery_percent);
	lv_obj_update_layout(well);
	const int32_t charged =
		(lv_obj_get_content_width(well) * model.battery_percent + 50) / 100;
	lv_obj_set_width(fill, std::max(charged, CHARGER_FILL_MIN));
	lv_obj_set_x(lv_obj_find_by_name(fill, "percent_on"),
				 lv_obj_get_x(percent));

	const lv_obj_t* line = lv_obj_find_by_name(transient_mode_screen, "line");
	lv_obj_t* text = lv_obj_find_by_name(line, "text");
	lv_obj_set_hidden(lv_obj_find_by_name(line, "bolt"),
					  model.battery != Battery::charging);
	if (model.battery == Battery::full) {
		lv_label_set_text(text, lv_tr("fully_charged"));
	} else if (model.battery_to_full_min == NO_ESTIMATE) {
		lv_label_set_text(text, lv_tr("charging"));
	} else {
		lv_label_set_text_fmt(
			text, "%u:%02u %s", model.battery_to_full_min / 60u,
			model.battery_to_full_min % 60u, lv_tr("until_full"));
	}
}

void show_mode(const ViewState& model) {
	if (model.mode != shown_mode) {
		lv_obj_t* left = transient_mode_screen;
		transient_mode_screen = model.mode == Mode::starting  ? startup_create()
								: model.mode == Mode::charger ? charger_create()
															  : nullptr;
		lv_screen_load(transient_mode_screen ? transient_mode_screen : home);
		if (left) {
			lv_obj_delete(left);
		}
		shown_mode = model.mode;
	}
	if (model.mode == Mode::starting) {
		show_startup(model);
	} else if (model.mode == Mode::charger) {
		show_charger(model);
	}
}

// The control has written the subject. A tap on what is chosen already
// changes nothing and asks for nothing.
template <auto action>
void bind_choice(lv_obj_t* control, lv_subject_t* subject) {
	lv_obj_add_event_cb(
		control,
		[](lv_event_t* event) {
			auto* value =
				static_cast<lv_subject_t*>(lv_event_get_user_data(event));
			const int32_t chosen = lv_subject_get_int(value);
			if (auto apply = actions.*action;
				apply && chosen != lv_subject_get_previous_int(value)) {
				apply(chosen != 0);
			}
		},
		LV_EVENT_CLICKED, subject);
}

void zoom(int steps) {
	auto shown = radar_range_index(
		static_cast<uint8_t>(lv_subject_get_int(subject_radar_range)));
	if (!shown || !actions.set_radar_range) {
		return;
	}
	const int wanted = *shown + steps;
	if (wanted >= 0 && std::cmp_less(wanted, std::size(RANGES_NM))) {
		actions.set_radar_range(RANGES_NM[wanted]);
	}
}

void apply_theme(bool dark) {
	const struct {
		lv_subject_t* token;
		lv_color_t light, dark;
	} tokens[] = {
		{color_bg, LIGHT_BG, DARK_BG},
		{color_bg_grouped, LIGHT_BG_GROUPED, DARK_BG_GROUPED},
		{color_surface, LIGHT_SURFACE, DARK_SURFACE},
		{color_separator, LIGHT_SEPARATOR, DARK_SEPARATOR},
		{color_label, LIGHT_LABEL, DARK_LABEL},
		{color_label_2, LIGHT_LABEL_2, DARK_LABEL_2},
		{color_tint, LIGHT_TINT, DARK_TINT},
		{color_green, LIGHT_GREEN, DARK_GREEN},
		{color_on_green, LIGHT_ON_GREEN, DARK_ON_GREEN},
		{color_logo_dot, LIGHT_LOGO_DOT, DARK_LOGO_DOT},
		{color_red, LIGHT_RED, DARK_RED},
		{color_red_dim, LIGHT_RED_DIM, DARK_RED_DIM},
		{color_yellow, LIGHT_YELLOW, DARK_YELLOW},
		{color_grid, LIGHT_GRID, DARK_GRID},
		{color_control, LIGHT_CONTROL, DARK_CONTROL},
		{color_pill, LIGHT_PILL, DARK_PILL},
		{color_pill_label, LIGHT_PILL_LABEL, DARK_PILL_LABEL},
		{color_pill_flarm, LIGHT_PILL_FLARM, DARK_PILL_FLARM},
		{color_pill_flarm_label, LIGHT_PILL_FLARM_LABEL, DARK_PILL_FLARM_LABEL},
	};
	for (const auto& token : tokens) {
		lv_subject_set_color(token.token, dark ? token.dark : token.light);
	}
}

void return_home(lv_timer_t*) {
	const auto page = static_cast<Page>(lv_subject_get_int(subject_page));
	if (lv_display_get_inactive_time(nullptr) >=
		page_settings(page).idle_return_ms) {
		navigate(HOME);
		traffic_table_scroll_to_top(table);
	}
}

void blink(lv_timer_t*) {
	const bool alert = lv_subject_get_int(subject_alert) != 0;
	lv_subject_set_int(subject_alert_phase,
					   alert && lv_subject_get_int(subject_alert_phase) == 0);
}

const void* symbol(Kind kind) {
	switch (kind) {
		case Kind::light:
			return symbol_light;
		case Kind::airliner:
			return symbol_airliner;
		case Kind::glider:
			return symbol_glider;
		case Kind::helicopter:
			return symbol_helicopter;
		case Kind::balloon:
			return symbol_balloon;
		case Kind::unknown:
			break;
	}
	return symbol_unknown;
}

traffic_source_t row_source(Source source) {
	switch (source) {
		case Source::adsb:
		case Source::adsb_other:
			return TRAFFIC_ADSB;
		case Source::adsr:
			return TRAFFIC_ADSR;
		case Source::tisb:
			return TRAFFIC_TISB;
		case Source::flarm:
			return TRAFFIC_FLARM;
		case Source::other:
			break;
	}
	return TRAFFIC_UNKNOWN_SOURCE;
}

traffic_level_t level(conflicts::Level value) {
	switch (value) {
		case conflicts::Level::other:
			return TRAFFIC_LEVEL_OTHER;
		case conflicts::Level::proximate:
			return TRAFFIC_LEVEL_PROXIMATE;
		case conflicts::Level::advisory:
			return TRAFFIC_LEVEL_ADVISORY;
		case conflicts::Level::alert:
			return TRAFFIC_LEVEL_ALERT;
	}
	return TRAFFIC_LEVEL_OTHER;
}

void show_wifi(const ViewState& model) {
	lv_subject_copy_string(subject_wifi_network, model.wifi_network);
	lv_subject_copy_string(subject_wifi_password, model.wifi_password);
	char join[160];
	const int length =
		std::snprintf(join, sizeof(join), "WIFI:T:WPA;S:%s;P:%s;;",
					  model.wifi_network, model.wifi_password);
	lv_qrcode_update(wifi_qr, join, static_cast<uint32_t>(length));
}

void show_battery(const ViewState& model) {
	const bool present = model.battery != Battery::absent;
	lv_subject_set_int(subject_battery_state,
					   static_cast<int32_t>(model.battery));
	lv_subject_set_int(subject_battery, present ? model.battery_percent : 0);
	if (present && model.battery_remaining_min != NO_ESTIMATE) {
		lv_subject_snprintf(subject_battery_remaining, "%u:%02u h",
							model.battery_remaining_min / 60u,
							model.battery_remaining_min % 60u);
	} else {
		lv_subject_copy_string(subject_battery_remaining, "–");
	}
}

constexpr uint32_t encode_row_id(TargetId target) {
	return static_cast<uint32_t>(target.source) << 24U | target.address;
}

constexpr TargetId decode_row_id(uint32_t id) {
	return {id & 0xFFFFFFU, static_cast<Source>(id >> 24U)};
}

void target_action(traffic_action_t action, uint32_t id) {
	const TargetId target = decode_row_id(id);
	if (action == TRAFFIC_ACTION_OWN_SHIP) {
		if (actions.set_own_ship) {
			actions.set_own_ship(target.address);
		}
	} else if (actions.set_muted) {
		actions.set_muted(target, action == TRAFFIC_ACTION_MUTE);
	}
}

conflicts::Level attention(const Target& target) {
	return target.muted ? conflicts::Level::other : target.level;
}

constexpr size_t MAX_BEYOND_ROWS = 10;

void to_radar(const Target& target, radar_target_t& out) {
	static_assert(RADAR_TAIL_HISTORY_S >= kTailSamples);
	out = {
		.symbol = symbol(target.kind),
		.level = level(target.level),
		.muted = target.muted,
		.track_valid = target.track_valid,
		.bearing_deg = static_cast<int16_t>(target.bearing_deg),
		.track_deg = static_cast<int16_t>(target.track_deg),
		.relative_altitude_ft = target.relative_altitude_ft.value_or(0),
		.relative_altitude_unknown = !target.relative_altitude_ft,
		.vertical_trend = static_cast<int8_t>(target.vertical_trend),
		.distance_nm = target.distance_nm,
		.predicted = target.predicted,
		.speed_kt = target.speed_kt,
		.turn_dps = target.turn_dps,
		.tail_count = target.tail_count,
		.tail = {},
	};
	for (size_t second = 0; second < target.tail_count; second++) {
		out.tail[second] = {target.tail[second].right_nm,
							target.tail[second].ahead_nm};
	}
}

void show_radar(std::span<const Target> inside, std::span<const Target> beyond,
				const ViewState& model) {
	// With their tails the targets are too large for the stack of a task.
	static std::array<radar_target_t, RADAR_MAX_TARGETS> targets;
	std::array<int16_t, RADAR_MAX_TARGETS> bearings{};
	std::array<traffic_level_t, RADAR_MAX_TARGETS> levels{};
	inside = inside.first(std::min(inside.size(), targets.size()));
	for (size_t i = 0; i < inside.size(); i++) {
		to_radar(inside[i], targets[i]);
	}
	size_t marked = 0;
	for (const Target& target : beyond) {
		if (target.show_beyond_ring_marker && marked < bearings.size()) {
			levels[marked] = level(attention(target));
			bearings[marked++] = static_cast<int16_t>(target.bearing_deg);
		}
	}
	radar_set_range_nm(radar, model.radar_range_nm);
	radar_set_tracks(radar, model.track_history);
	radar_set_simple(radar, model.simple_symbols);
	radar_set_targets(radar, targets.data(),
					  static_cast<uint32_t>(inside.size()));
	radar_set_beyond(radar, bearings.data(), levels.data(),
					 static_cast<uint32_t>(marked));
}

radar_band_level_t radar_region(conflicts::BandRegion region) {
	switch (region) {
		case conflicts::BandRegion::preventive:
			return RADAR_BAND_PREVENTIVE;
		case conflicts::BandRegion::corrective:
			return RADAR_BAND_CORRECTIVE;
		case conflicts::BandRegion::warning:
			return RADAR_BAND_WARNING;
		case conflicts::BandRegion::recovery:
			return RADAR_BAND_RECOVERY;
	}
	return RADAR_BAND_NONE;
}

void show_tape(radar_tape_t tape, std::span<const conflicts::Band> source,
			   float units_per_half_tape) {
	std::array<radar_tape_band_t, RADAR_MAX_BANDS> bands{};
	size_t count = 0;
	for (const conflicts::Band& band :
		 source.first(std::min(source.size(), bands.size()))) {
		bands[count++] = {band.from / units_per_half_tape,
						  band.to / units_per_half_tape,
						  radar_region(band.region)};
	}
	radar_set_tape(radar, tape, bands.data(), static_cast<uint32_t>(count));
}

// A tape spans what the engine tries to either side of the own value: beyond
// it nothing is known, and the tape must not show that as free.
void show_bands(const ViewState& model) {
	std::array<radar_track_band_t, RADAR_MAX_BANDS> bands{};
	size_t count = 0;
	for (const conflicts::Band& band : model.track_bands.first(
			 std::min(model.track_bands.size(), bands.size()))) {
		bands[count++] = {band.from, band.to, radar_region(band.region)};
	}
	radar_set_indicator(radar, model.conflict_indicator);
	radar_set_track_bands(radar, bands.data(), static_cast<uint32_t>(count));
	show_tape(RADAR_TAPE_ALTITUDE, model.altitude_bands,
			  conflicts::kAltitudeBandSpanFt);
	show_tape(RADAR_TAPE_SPEED, model.speed_bands, conflicts::kSpeedBandSpanKt);
}

void show_table(std::span<const Target> targets, size_t inside,
				const ViewState& model) {
	static conflicts::Level shown = conflicts::Level::other;
	conflicts::Level highest = conflicts::Level::other;
	for (const Target& target : targets) {
		highest = std::max(highest, attention(target));
	}
	const bool scroll_on_escalation =
		highest > shown && highest >= conflicts::Level::advisory;
	if (scroll_on_escalation) {
		traffic_table_scroll_to_top(table);
	}
	shown = highest;

	std::array<traffic_row_t, TRAFFIC_TABLE_MAX_ROWS> rows{};
	std::array<char[7], TRAFFIC_TABLE_MAX_ROWS> addresses{};
	targets = targets.first(std::min(targets.size(), rows.size()));
	for (size_t i = 0; i < targets.size(); i++) {
		const Target& target = targets[i];
		(void)std::snprintf(addresses[i], sizeof(addresses[i]), "%06X",
							static_cast<unsigned>(target.address));
		rows[i] = {
			.target_id = encode_row_id({target.address, target.source}),
			.title = target.callsign[0] ? target.callsign : addresses[i],
			.source = row_source(target.source),
			.level = level(target.level),
			.muted = target.muted,
			.own_ship_offered = target.own_ship_candidate,
			.relative_position_available =
				target.radar_position != RadarPosition::unknown,
			.relative_altitude_ft = target.relative_altitude_ft.value_or(0),
			.relative_altitude_unknown = !target.relative_altitude_ft,
			.vertical_trend = static_cast<int8_t>(target.vertical_trend),
			.relative_bearing_deg = static_cast<int16_t>(target.bearing_deg),
			.distance_nm = target.distance_nm,
			.conflict_in_s = target.conflict_in_s,
		};
	}
	char band[TRAFFIC_TABLE_BAND_SIZE];
	(void)std::snprintf(band, sizeof(band), "%s %u NM", lv_tr("beyond"),
						model.radar_range_nm);
	traffic_table_set_band(table, band);
	traffic_table_set_rows(table, rows.data(),
						   static_cast<uint32_t>(targets.size()),
						   static_cast<uint32_t>(inside));
}

void show_traffic(const ViewState& model) {
	const size_t inside = std::min(model.traffic_inside, model.traffic.size());
	if (model.fix == Fix::none) {
		show_radar({}, {}, model);
		show_table(model.traffic, model.traffic.size(), model);
		return;
	}
	show_radar(model.traffic.first(inside), model.traffic.subspan(inside),
			   model);
	const size_t rows =
		std::min(inside + MAX_BEYOND_ROWS, model.traffic.size());
	show_table(model.traffic.first(rows), inside, model);
}
}

// The user interface is the LVGL Pro project in ui/. It brings its own
// screens, the one LVGL created for the panel stays unused.
void variant::build(lv_obj_t*, const Actions& handlers) {
	actions = handlers;
	airgrep_ui_init(nullptr);
	bind_choice<&Actions::set_dark_theme>(find("page_system", "light"),
										  subject_theme_dark);
	bind_choice<&Actions::set_dark_theme>(find("page_system", "dark"),
										  subject_theme_dark);
	bind_choice<&Actions::set_simple_symbols>(find("page_system", "planes"),
											  subject_simple_symbols);
	bind_choice<&Actions::set_simple_symbols>(find("page_system", "simple"),
											  subject_simple_symbols);
	bind_choice<&Actions::set_track_history>(
		find("page_system", "track_history"), subject_track_history);
	bind_choice<&Actions::set_track_history>(lv_obj_find_by_name(home, "track"),
											 subject_track_history);
	bind_choice<&Actions::set_conflict_indicator>(
		find("page_system", "conflict_indicator"), subject_conflict_indicator);
	bind_choice<&Actions::set_time_zone_automatic>(
		find("page_system", "time_zone_automatic"),
		subject_time_zone_automatic);
	lv_obj_add_event_cb(
		find("page_system", "time_zone"),
		[](lv_event_t*) {
			draw_choice();
			lv_obj_scroll_to_y(find("page_time_zone", "sheet"), 0, LV_ANIM_OFF);
			navigate(TIME_ZONE);
		},
		LV_EVENT_CLICKED, nullptr);
	radar = lv_obj_find_by_name(home, "radar");
	table = lv_obj_find_by_name(home, "traffic");
	wifi_qr = lv_obj_find_by_name(home, "wifi_qr");
	traffic_table_set_action_cb(table, target_action);
	lv_obj_t* to_licenses = find("page_system", "licenses");
	lv_obj_set_clickable(to_licenses, true);
	lv_obj_add_event_cb(
		to_licenses, [](lv_event_t*) { navigate(LICENSES); }, LV_EVENT_CLICKED,
		nullptr);
	for (Entry* entry : ENTRIES) {
		entry->field = find(entry->page_name, "field");
		entry->keys = find(entry->page_name, "keys");
		keyboard_set_cb(entry->keys, key_tapped);
		lv_obj_t* to_entry = find("page_system", entry->row_name);
		lv_obj_set_clickable(to_entry, true);
		lv_obj_add_event_cb(to_entry, open_entry, LV_EVENT_CLICKED, entry);
	}
	for (const char* page : {"page_system", "page_licenses", "page_license",
							 "page_time_zone", "page_zones"}) {
		lv_obj_add_event_cb(find(page, "sheet"), keep_inside, LV_EVENT_SCROLL,
							nullptr);
	}
	for (lv_indev_t* input = lv_indev_get_next(nullptr); input;
		 input = lv_indev_get_next(input)) {
		lv_indev_set_scroll_limit(input, SCROLL_LOCK);
	}
	lv_timer_create(blink, BLINK_HALF_MS, nullptr);
	lv_timer_create(return_home, 1000, nullptr);
	lv_screen_load(home);
}

void update(const ViewState& model) {
	apply_theme(model.dark_theme);
	lv_subject_set_int(subject_theme_dark, model.dark_theme);
	lv_subject_set_int(subject_track_history, model.track_history);
	lv_subject_set_int(subject_simple_symbols, model.simple_symbols);
	lv_subject_set_int(subject_conflict_indicator, model.conflict_indicator);
	lv_subject_set_int(subject_time_zone_automatic, model.time_zone_automatic);
	lv_subject_set_int(subject_radar_range, model.radar_range_nm);
	lv_subject_set_int(subject_antenna, static_cast<int32_t>(model.antenna));
	lv_subject_set_int(subject_health,
					   static_cast<int32_t>(level(model.causes)));
	const int32_t symbol = gps_symbol(model);
	lv_subject_set_int(subject_gps, static_cast<int32_t>(model.fix));
	lv_subject_set_int(subject_gps_symbol, symbol);
	lv_subject_set_int(subject_satellites, model.satellites);
	lv_subject_set_int(
		subject_signal,
		symbol == 5 ? -1 : model.strongest_satellite_signal_level);
	lv_subject_snprintf(subject_time, "%02u:%02u", model.hour, model.minute);
	lv_subject_copy_string(subject_time_zone, model.zone_abbreviation);
	lv_subject_copy_string(subject_device_id, model.device_id);
	lv_subject_set_int(subject_alert, model.alert);
	lv_subject_set_int(subject_wifi, model.wifi_on);
	lv_subject_set_int(subject_muted, model.muted);
	lv_subject_copy_string(subject_version, model.version);
	show_language(model);
	show_health(model);
	show_mode(model);
	show_wifi(model);
	show_battery(model);
	show_own_ship(model);
	show_filter(model);
	show_licenses(model);
	show_time_zone(model);
	show_traffic(model);
	show_bands(model);
}
}

extern "C" {
void header_left_clicked(lv_event_t*) { display::navigate(display::SYSTEM); }
void header_right_clicked(lv_event_t*) {
	display::navigate(display::WIFI_BATTERY);
}
void zoom_out_clicked(lv_event_t*) { display::zoom(1); }
void zoom_in_clicked(lv_event_t*) { display::zoom(-1); }
}

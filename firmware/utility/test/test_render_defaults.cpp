#include <zlib.h>

#include <algorithm>
#include <array>
#include <vector>

#include "airgrep_ui.h"
#include "check.hpp"
#include "display/display.hpp"
#include "widgets/radar/radar_outline.h"
#include "widgets/radar/radar_private_gen.h"

namespace {
unsigned flushes;

void advance(lv_display_t* panel, uint32_t ms) {
	lv_tick_inc(ms);
	lv_anim_refr_now();
	lv_refr_now(panel);
}

uint32_t atlas_masks() {
	uint32_t masks = 0, bytes = 0;
	radar_symbol_atlas_usage(&masks, &bytes);
	return masks;
}

void run_timers(uint32_t ms) {
	lv_tick_inc(ms);
	lv_timer_handler();
}

void startup_symbols(lv_display_t* panel, std::span<uint16_t> pixels) {
	using display::Kind;
	using display::Mode;
	constexpr uint32_t masks_per_slice = 24;
	display::ViewState model;
	model.fix = display::Fix::fix_3d;
	constexpr std::array kinds{Kind::unknown, Kind::light,      Kind::airliner,
							   Kind::glider,  Kind::helicopter, Kind::balloon};
	std::array<display::Target, kinds.size()> targets{};
	for (size_t i = 0; i < targets.size(); ++i) {
		auto& target = targets[i];
		target.kind = kinds[i];
		target.address = static_cast<uint32_t>(0xabc000 + i);
		target.distance_nm = 2;
		target.bearing_deg = static_cast<uint16_t>(60 * i);
		target.track_deg = static_cast<uint16_t>(10 * i + 7);
		target.track_valid = i != 0;
		target.radar_position = display::RadarPosition::inside_ring;
	}
	model.traffic = targets;
	model.traffic_inside = targets.size();
	const auto show = [&](Mode mode) {
		model.mode = mode;
		display::update(model);
		lv_refr_now(panel);
	};
	const auto welcome = [&] {
		radar_symbol_atlas_release();
		const unsigned before = flushes;
		model.mode = Mode::starting;
		display::update(model);
		auto* refresh = lv_display_get_refr_timer(panel);
		lv_timer_pause(refresh);
		run_timers(10);
		CHECK_EQUAL(before, flushes);
		CHECK_EQUAL(0U, atlas_masks());
		lv_timer_resume(refresh);
		lv_refr_now(panel);
		CHECK(flushes > before);
		CHECK(lv_obj_find_by_name(lv_display_get_screen_active(panel),
								  "greeting") != nullptr);
		CHECK_EQUAL(0U, atlas_masks());
	};

	show(Mode::charger);
	run_timers(2100);
	CHECK_EQUAL(0U, atlas_masks());
	for (const auto next : {Mode::on, Mode::charger}) {
		welcome();
		run_timers(5);
		CHECK(atlas_masks() > 0 && atlas_masks() <= masks_per_slice);
		show(next);
		const auto before = atlas_masks();
		run_timers(100);
		CHECK_EQUAL(before, atlas_masks());
	}

	welcome();
	run_timers(1900);
	CHECK(atlas_masks() > 0 && atlas_masks() <= masks_per_slice);
	model.simple_symbols = true;
	display::update(model);
	const auto before_deadline = atlas_masks();
	run_timers(101);
	run_timers(5);
	CHECK_EQUAL(before_deadline, atlas_masks());
	show(Mode::charger);

	for (const bool simple : {true, false}) {
		model.simple_symbols = simple;
		welcome();
		const uint32_t expected = simple ? 76 : 434;
		for (unsigned elapsed = 0; elapsed < 2000 && atlas_masks() < expected;
			 elapsed += 5) {
			const auto before = atlas_masks();
			run_timers(5);
			CHECK(atlas_masks() <= before + masks_per_slice);
		}
		CHECK_EQUAL(expected, atlas_masks());
		uint32_t hits = 0, misses = 0;
		radar_symbol_atlas_lookups(&hits, &misses);
		const auto warm_hits = hits, warm_misses = misses;
		show(Mode::on);
		radar_symbol_atlas_lookups(&hits, &misses);
		CHECK(hits > warm_hits);
		CHECK_EQUAL(warm_misses, misses);
		const std::vector<uint16_t> warm_home(pixels.begin(), pixels.end());
		show(Mode::charger);
		radar_symbol_atlas_release();
		show(Mode::on);
		radar_symbol_atlas_lookups(&hits, &misses);
		CHECK(misses > 0);
		CHECK(std::ranges::equal(pixels, warm_home));
	}
}

void count_tasks(lv_obj_t* obj, unsigned& count) {
	lv_obj_set_send_draw_task_events(obj, true);
	lv_obj_add_event_cb(
		obj,
		[](lv_event_t* e) {
			++*static_cast<unsigned*>(lv_event_get_user_data(e));
		},
		LV_EVENT_DRAW_TASK_ADDED, &count);
}

void covered_home(lv_display_t* panel, std::span<uint16_t> pixels,
				  display::ViewState& model) {
	static unsigned home_tasks, bottom_tasks;
	count_tasks(home, home_tasks);
	count_tasks(lv_display_get_layer_bottom(panel), bottom_tasks);
	display::Target contact{};
	contact.address = 0xabc123;
	model.traffic = std::span(&contact, 1);
	model.traffic_inside = 1;
	model.radar_generations.emplace();
	std::ranges::copy("LATEST", contact.callsign);
	display::update(model);
	lv_refr_now(panel);
	const std::vector<uint16_t> expected_home(pixels.begin(), pixels.end());
	std::ranges::copy("EARLIER", contact.callsign);
	display::update(model);
	lv_refr_now(panel);
	CHECK(!std::ranges::equal(pixels, expected_home));

	auto* table = lv_obj_find_by_name(home, "traffic");
	auto* system = lv_obj_find_by_name(home, "page_system");
	lv_obj_send_event(lv_obj_find_by_name(home, "touch_left"), LV_EVENT_CLICKED,
					  nullptr);
	advance(panel, 48);
	CHECK(!lv_obj_is_hidden(table));
	advance(panel, 300);
	CHECK(lv_obj_is_hidden(table));
	for (const bool dark : {true, false}) {
		model.dark_theme = dark;
		display::update(model);
		lv_refr_now(panel);
		const std::vector<uint16_t> expected(pixels.begin(), pixels.end());
		for (auto& pixel : pixels) {
			pixel ^= 0xffff;
		}
		home_tasks = bottom_tasks = 0;
		lv_obj_invalidate(home);
		lv_refr_now(panel);
		CHECK(std::ranges::equal(pixels, expected));
		CHECK_EQUAL(0U, home_tasks);
		CHECK_EQUAL(0U, bottom_tasks);
	}
	const unsigned before = flushes;
	std::ranges::copy("LATEST", contact.callsign);
	display::update(model);
	lv_refr_now(panel);
	CHECK_EQUAL(before, flushes);

	lv_obj_send_event(lv_obj_find_by_name(system, "licenses"), LV_EVENT_CLICKED,
					  nullptr);
	advance(panel, 350);
	CHECK(lv_obj_is_hidden(system));
	auto* licenses = lv_obj_find_by_name(home, "page_licenses");
	lv_obj_send_event(lv_obj_find_by_name(licenses, "back_button"),
					  LV_EVENT_CLICKED, nullptr);
	advance(panel, 48);
	CHECK(!lv_obj_is_hidden(system));
	CHECK(lv_obj_is_hidden(table));
	advance(panel, 300);

	lv_obj_send_event(lv_obj_find_by_name(system, "back_button"),
					  LV_EVENT_CLICKED, nullptr);
	advance(panel, 48);
	CHECK(!lv_obj_is_hidden(table));
	advance(panel, 300);
	CHECK(std::ranges::equal(pixels, expected_home));
	home_tasks = 0;
	lv_obj_invalidate(home);
	lv_refr_now(panel);
	CHECK(home_tasks > 0);
}

bool has_direct_digit(const lv_font_t* font) {
	lv_font_glyph_dsc_t digit{};
	return font && lv_font_has_static_bitmap(font) &&
		   lv_font_get_glyph_dsc(font, &digit, '0', 0) &&
		   digit.format == LV_FONT_GLYPH_FORMAT_A8 &&
		   lv_font_get_glyph_static_bitmap(&digit) != nullptr;
}

void default_grid(lv_display_t* panel, std::span<const uint16_t> pixels) {
	auto* radar =
		reinterpret_cast<radar_t*>(lv_obj_find_by_name(home, "radar"));
	CHECK(radar->sparse_grid != nullptr);
	display::Target target{};
	target.address = 0xabc001;
	target.kind = display::Kind::light;
	target.distance_nm = 2;
	target.radar_position = display::RadarPosition::inside_ring;
	target.tail_count = 12;
	for (unsigned i = 0; i < target.tail_count; ++i) {
		target.tail[i] = {0.1f * static_cast<float>(i + 1), 2};
	}
	display::ViewState model;
	model.fix = display::Fix::fix_3d;
	model.track_history = true;
	model.traffic = std::span(&target, 1);
	model.traffic_inside = 1;
	display::update(model);
	lv_obj_invalidate(home);
	lv_refr_now(panel);
	CHECK(radar->sparse_grid != nullptr);
	// Full RGB565 frame from the original LVGL line and circle renderer.
	CHECK_EQUAL(0x12b61a01UL,
				crc32_z(0, reinterpret_cast<const Bytef*>(pixels.data()),
						pixels.size_bytes()));
}

void home_with_recovery() {
	lv_init();
	auto* panel = lv_display_create(800, 480);
	static std::array<uint16_t, 800zu * 480> pixels;
	lv_display_set_buffers(panel, pixels.data(), nullptr, sizeof(pixels),
						   LV_DISPLAY_RENDER_MODE_DIRECT);
	lv_display_set_flush_cb(
		panel, [](lv_display_t* display, const lv_area_t*, uint8_t*) {
			++flushes;
			lv_display_flush_ready(display);
		});
	display::create(panel, {});
	CHECK(has_direct_digit(font_label));
	CHECK(has_direct_digit(font_small));
	CHECK(has_direct_digit(font_text));
	CHECK(has_direct_digit(font_title));
	CHECK(has_direct_digit(font_data));
	CHECK(has_direct_digit(font_display));
	CHECK(has_direct_digit(font_stored));
	startup_symbols(panel, pixels);

	const conflicts::Band recovery[] = {
		{0, 360, conflicts::BandRegion::recovery}};
	display::ViewState model;
	model.wifi_network = "";
	model.wifi_password = "";
	model.fix = display::Fix::fix_3d;
	model.conflict_indicator = true;
	model.track_bands = recovery;
	display::update(model);
	lv_refr_now(panel);
	auto* radar =
		reinterpret_cast<radar_t*>(lv_obj_find_by_name(home, "radar"));
	CHECK(radar->recovery_sectors != nullptr);
	default_grid(panel, pixels);
	covered_home(panel, pixels, model);
	lv_deinit();
}
}

int main() {
	return check::run({{"800x480 production rendering", home_with_recovery}});
}

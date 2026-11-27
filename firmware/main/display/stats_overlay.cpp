#include "stats_overlay.hpp"

#include "esp_heap_caps.h"
#include "esp_timer.h"

namespace display {
namespace {
struct Stats {
	lv_display_t* panel{};
	lv_obj_t* label{};
	int64_t window{}, started{}, total{};
	unsigned frames{};
};

void refresh(lv_timer_t* timer) {
	auto& stats = *static_cast<Stats*>(lv_timer_get_user_data(timer));
	const auto now = esp_timer_get_time();
	const auto elapsed = now - stats.window;
	const auto* screen = lv_display_get_screen_active(stats.panel);
	const bool light = lv_color_luminance(lv_obj_get_style_bg_color(
						   screen, LV_PART_MAIN)) >= 128;
	lv_obj_set_style_bg_color(stats.label,
							  light ? lv_color_white() : lv_color_black(), 0);
	lv_obj_set_style_text_color(stats.label,
								light ? lv_color_black() : lv_color_white(), 0);
	const unsigned fps = static_cast<unsigned>(
		(stats.frames * 1000000ULL + elapsed / 2) / elapsed);
	const unsigned ms =
		stats.frames ? static_cast<unsigned>(stats.total / stats.frames / 1000)
					 : 0;
	lv_label_set_text_fmt(
		stats.label, "%u fps  %u ms  %u KiB", fps, ms,
		static_cast<unsigned>(
			heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) /
			1024));
	stats.window = now;
	stats.total = 0;
	stats.frames = 0;
}
}  // namespace
void attach_stats_overlay(lv_display_t* panel) {
	auto* label = lv_label_create(lv_display_get_layer_top(panel));
	static Stats stats{panel, label, esp_timer_get_time(), 0};
	lv_obj_set_clickable(label, false);
	lv_obj_set_style_pad_all(label, 3, 0);
	lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
	lv_obj_align(label, LV_ALIGN_BOTTOM_RIGHT, -4, -4);
	lv_label_set_text(label, "");
	lv_display_add_event_cb(
		panel, [](lv_event_t*) { stats.started = esp_timer_get_time(); },
		LV_EVENT_RENDER_START, nullptr);
	lv_display_add_event_cb(
		panel,
		[](lv_event_t*) {
			stats.total += esp_timer_get_time() - stats.started;
			++stats.frames;
		},
		LV_EVENT_RENDER_READY, nullptr);
	lv_timer_create(refresh, 1000, &stats);
}
}  // namespace display

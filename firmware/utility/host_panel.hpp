#pragma once

#include <cstdint>
#include <span>

#include "lvgl.h"

namespace host {
struct Pointer {
	lv_point_t point{};
	bool pressed = false;
};

// The caller keeps the RGB565 buffer and pointer alive for the display.
inline lv_display_t* create_panel(int32_t width, int32_t height,
								  std::span<uint16_t> buffer,
								  lv_display_flush_cb_t flush,
								  Pointer& pointer) {
	lv_display_t* panel = lv_display_create(width, height);
	lv_display_set_buffers(panel, buffer.data(), nullptr, buffer.size_bytes(),
						   LV_DISPLAY_RENDER_MODE_DIRECT);
	lv_display_set_flush_cb(panel, flush);

	lv_indev_t* touch = lv_indev_create();
	lv_indev_set_type(touch, LV_INDEV_TYPE_POINTER);
	lv_indev_set_user_data(touch, &pointer);
	lv_indev_set_read_cb(touch, [](lv_indev_t* device, lv_indev_data_t* data) {
		const auto& input =
			*static_cast<const Pointer*>(lv_indev_get_user_data(device));
		data->point = input.point;
		data->state =
			input.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
	});
	return panel;
}
}

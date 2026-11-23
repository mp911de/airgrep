#include "display.h"

#include <string>
#include <vector>

#include "connection.hpp"
#include "display/display.hpp"
#include "host_panel.hpp"
#include "remote/view.hpp"
#include "rgb565.hpp"

namespace {
std::vector<uint16_t> rgb565_framebuffer;
std::vector<uint8_t> bgrx;
bool changed;
uint32_t elapsed;
host::Pointer pointer;

void flush(lv_display_t* panel, const lv_area_t* area, uint8_t*) {
	for (int32_t y = area->y1; y <= area->y2; y++) {
		for (int32_t x = area->x1; x <= area->x2; x++) {
			const size_t at = static_cast<size_t>(y) * display::WIDTH + x;
			const unsigned colour = rgb565_framebuffer[at];
			bgrx[at * 4] = rgb565::expand5(colour & 0x1fU);
			bgrx[at * 4 + 1] = rgb565::expand6(colour >> 5U & 0x3fU);
			bgrx[at * 4 + 2] = rgb565::expand5(colour >> 11U);
		}
	}
	changed = true;
	lv_display_flush_ready(panel);
}

void ensure_display_initialized() {
	static bool started = false;
	if (started) {
		return;
	}
	started = true;
	const size_t count = static_cast<size_t>(display::WIDTH) * display::HEIGHT;
	rgb565_framebuffer.assign(count, 0);
	bgrx.assign(count * 4, 0xff);
	lv_init();
	lv_tick_set_cb([] { return elapsed; });
	lv_display_t* panel = host::create_panel(
		display::WIDTH, display::HEIGHT, rgb565_framebuffer, flush, pointer);
	display::create(panel, remote::view::actions());
}
void follow(airgrep_connection* connection, const char* serial) {
	ensure_display_initialized();
	remote::view::follow(connection->connection, serial);
}

bool step(uint32_t elapsed_ms) {
	ensure_display_initialized();
	elapsed += elapsed_ms;
	changed = false;
	lv_timer_handler();
	return changed;
}

const uint8_t* panel_pixels() {
	ensure_display_initialized();
	return bgrx.data();
}

void touch(int32_t x, int32_t y, bool down) { pointer = {{x, y}, down}; }

const char* explained() { return remote::view::explained().c_str(); }

constexpr airgrep_display_api api{.width = display::WIDTH,
								  .height = display::HEIGHT,
								  .follow = follow,
								  .step = step,
								  .pixels = panel_pixels,
								  .touch = touch,
								  .muted = remote::view::muted,
								  .explained = explained};
}

const airgrep_display_api* airgrep_display_get_api(uint32_t version,
												   size_t size) {
	return version == AIRGREP_DISPLAY_ABI && size == sizeof(api) ? &api
																 : nullptr;
}

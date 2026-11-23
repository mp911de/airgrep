#include "display/display.hpp"
#include "variant.hpp"

namespace display {
namespace {
constexpr uint32_t BACKGROUND = 0x101418;
}

void create(lv_display_t* panel, const Actions& actions) {
	lv_obj_t* screen = lv_display_get_screen_active(panel);
	lv_obj_set_style_bg_color(screen, lv_color_hex(BACKGROUND), 0);
	lv_obj_set_style_text_color(screen, lv_color_white(), 0);
	variant::build(screen, actions);
}
}

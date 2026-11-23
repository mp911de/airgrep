#include "display/display.hpp"
#include "variant.hpp"

namespace display {
namespace {
lv_obj_t* status;
}

void variant::build(lv_obj_t* screen, const Actions&) {
	lv_obj_t* title = lv_label_create(screen);
	lv_label_set_text(title, "seeed-172x320-touch");
	lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 24);

	status = lv_label_create(screen);
	lv_obj_center(status);
}

void update(const ViewState& model) {
	lv_label_set_text_fmt(status, "Battery %u %%\nGPS %s\nAlert %s",
						  static_cast<unsigned>(model.battery_percent),
						  model.fix != Fix::none ? "fix" : "none",
						  model.alert ? "yes" : "no");
}
}

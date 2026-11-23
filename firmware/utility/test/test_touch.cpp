#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "app/airgrep.h"
#include "gestures.hpp"
#include "lvgl.h"
#include "src/lvgl_private.h"

// No object takes a touch from the target it lies in. A target is an object
// that answers a finger. LVGL hands a touch to the innermost clickable object
// under it, and an object that neither answers nor passes the touch on to the
// target around it leaves a place on the target where a tap does nothing.

namespace {
using namespace std::chrono_literals;
using gestures::Gesture;

constexpr char kSerial[] = "a1b2c3d4e5f6";
constexpr uint32_t kStepMs = 10, kSettleMs = 2500;
// Longer than the 30 s after which every page returns to Home.
constexpr uint32_t kHomeAgainMs = 35000;

struct Page {
	const char* name;
	std::vector<const char*> gestures;
};

// The pages of the variant and the way to each from Home, as
// tools/check-behaviour.py reaches them.
std::vector<Page> pages() {
	if (std::string_view(AIRGREP_DISPLAY) != "800x480-touch") {
		return {{"home", {}}};
	}
	constexpr const char* left = "tap:100,32";
	constexpr const char* to_end = "drag:200,460,200,70";
	// System keeps where it was scrolled to, so its rows come before its end.
	return {
		{"home", {}},
		{"home, row swiped left", {"drag:700,128,540,128"}},
		{"home, row swiped right", {"drag:500,128,660,128"}},
		{"wifi and battery", {"tap:700,32"}},
		{"system", {left}},
		{"altitude", {left, "tap:600,192"}},
		{"callsign", {left, "tap:600,291"}},
		{"icao address", {left, "tap:600,354"}},
		{"time zone", {left, "tap:200,256"}},
		{"time zone, region", {left, "tap:200,256", "tap:200,352"}},
		{"system, at its end", {left, to_end}},
		{"licenses", {left, to_end, "tap:200,448"}},
		{"license", {left, to_end, "tap:200,448", "tap:200,192"}},
	};
}

bool answers(lv_obj_t* object) {
	constexpr lv_event_code_t touched[] = {
		LV_EVENT_ALL,           LV_EVENT_PRESSED,        LV_EVENT_PRESSING,
		LV_EVENT_SHORT_CLICKED, LV_EVENT_SINGLE_CLICKED, LV_EVENT_CLICKED,
		LV_EVENT_LONG_PRESSED,  LV_EVENT_RELEASED,       LV_EVENT_GESTURE,
		LV_EVENT_VALUE_CHANGED};
	for (uint32_t i = 0; i < lv_obj_get_event_count(object); i++) {
		const lv_event_dsc_t* handler = lv_obj_get_event_dsc(object, i);
		const auto code = static_cast<lv_event_code_t>(
			handler->filter & ~static_cast<uint32_t>(LV_EVENT_PREPROCESS));
		if (std::ranges::find(touched, code) != std::end(touched)) {
			return true;
		}
	}
	// A control of LVGL takes its touches in its class: a switch, a keyboard.
	for (const lv_obj_class_t* kind = lv_obj_get_class(object);
		 kind && kind != &lv_obj_class; kind = kind->base_class) {
		if (kind->group_def == LV_OBJ_CLASS_GROUP_DEF_TRUE) {
			return true;
		}
	}
	return false;
}

bool shown(lv_obj_t* object) {
	lv_area_t area;
	lv_obj_get_coords(object, &area);
	const lv_area_t panel{
		0, 0,
		lv_display_get_horizontal_resolution(lv_display_get_default()) - 1,
		lv_display_get_vertical_resolution(lv_display_get_default()) - 1};
	return lv_obj_is_visible(object) && lv_area_intersect(&area, &area, &panel);
}

// The target a touch on the object is taken from, or none: the object
// answers itself, no target lies around it, or the touch bubbles up to it.
lv_obj_t* robbed(lv_obj_t* object) {
	if (!lv_obj_is_clickable(object) || answers(object) || !shown(object)) {
		return nullptr;
	}
	bool bubbles = true;
	for (lv_obj_t* around = object; around;
		 around = lv_obj_get_parent(around)) {
		if (around != object && answers(around)) {
			return bubbles ? nullptr : around;
		}
		bubbles = bubbles && lv_obj_is_event_bubble(around);
	}
	return nullptr;
}

std::string described(lv_obj_t* object) {
	std::string path;
	for (lv_obj_t* at = object; at; at = lv_obj_get_parent(at)) {
		const char* name = lv_obj_get_name(at);
		const char* kind = lv_obj_get_class(at)->name;
		path.insert(0, path.empty() ? "" : "/");
		path.insert(0, name ? name : kind ? kind : "object");
	}
	return path;
}

struct Audit {
	int targets = 0, findings = 0;
};

void audit(const char* page, lv_obj_t* object, Audit& found) {
	found.targets += shown(object) && answers(object);
	if (lv_obj_t* target = robbed(object)) {
		lv_area_t area;
		lv_obj_get_coords(object, &area);
		std::printf("%s: %s takes the touch of %s at %d,%d to %d,%d\n", page,
					described(object).c_str(), described(target).c_str(),
					static_cast<int>(area.x1), static_cast<int>(area.y1),
					static_cast<int>(area.x2), static_cast<int>(area.y2));
		found.findings++;
	}
	for (uint32_t i = 0; i < lv_obj_get_child_count(object); i++) {
		audit(page, lv_obj_get_child(object, static_cast<int32_t>(i)), found);
	}
}

// The audit has to find an object that takes a touch: a plain object is
// clickable and passes nothing on.
bool finds_a_planted_thief() {
	lv_obj_t* target = lv_button_create(lv_layer_top());
	lv_obj_add_event_cb(target, [](lv_event_t*) {}, LV_EVENT_CLICKED, nullptr);
	lv_obj_create(target);
	lv_obj_update_layout(target);
	Audit found;
	audit("planted", target, found);
	lv_obj_delete(target);
	return found.findings == 1;
}
}

int main() {
	const airgrep_display_api* display = airgrep_display_get_api(
		AIRGREP_DISPLAY_ABI, sizeof(airgrep_display_api));
	// A manual zone, so the Time zone row takes its tap.
	airgrep_connection* connection =
		airgrep_connect_fixture("advisory zone=Europe/Berlin");
	airgrep_watch(connection, 20);
	const auto pass = [&](uint32_t display_ms) {
		display->follow(connection, kSerial);
		display->step(display_ms);
	};
	airgrep_status status{};
	const auto deadline = std::chrono::steady_clock::now() + 5s;
	while (status.pictures_read < 3 && status.phase != AIRGREP_ENDED &&
		   std::chrono::steady_clock::now() < deadline) {
		pass(0);
		std::this_thread::sleep_for(1ms);
		airgrep_status_of(connection, &status);
	}
	int findings = status.pictures_read < 3;
	if (findings) {
		std::printf("The device sent nothing: %s\n", status.error);
	}
	if (!findings && !finds_a_planted_thief()) {
		std::printf("The audit misses an object that takes a touch\n");
		findings++;
	}
	for (const Page& page : pages()) {
		for (uint32_t at = 0; !findings && at < kHomeAgainMs; at += 100) {
			pass(100);
		}
		for (const char* text : page.gestures) {
			Gesture gesture;
			if (!gestures::parse(text, gesture)) {
				std::printf("%s: %s is no gesture\n", page.name, text);
				findings++;
			}
			for (uint32_t at = 0; at < gestures::kGestureMs; at += kStepMs) {
				gestures::touch(*display, gesture, at);
				pass(kStepMs);
			}
		}
		for (uint32_t at = 0; at < kSettleMs; at += kStepMs) {
			pass(kStepMs);
		}
		Audit found;
		audit(page.name, lv_screen_active(), found);
		audit(page.name, lv_layer_top(), found);
		std::printf("%s: %d targets\n", page.name, found.targets);
		findings += found.findings;
	}
	airgrep_watch(connection, 0);
	airgrep_disconnect(connection);
	std::printf("%d places where a tap is lost\n", findings);
	return findings ? 1 : 0;
}

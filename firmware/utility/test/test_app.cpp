#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "800x480-touch/display_settings.hpp"
#include "app/airgrep.h"
#include "app/display.h"
#include "check.hpp"
#include "device/emulator.hpp"
#include "display_loader.hpp"
#include "remote/settings_edits.hpp"

namespace {
using namespace std::chrono_literals;

airgrep_display_getter_ptr get_display;
// The day of the time the device says. No test here reads a clock.
constexpr int64_t kDay = 20'468;

void negotiates_the_display_abi() {
	CHECK(get_display(AIRGREP_DISPLAY_ABI + 1, sizeof(airgrep_display_api)) ==
		  nullptr);
	CHECK(get_display(AIRGREP_DISPLAY_ABI, sizeof(airgrep_display_api) - 1) ==
		  nullptr);
	const auto* api =
		get_display(AIRGREP_DISPLAY_ABI, sizeof(airgrep_display_api));
	CHECK(api != nullptr);
	if (api) {
		CHECK(api->width > 0 && api->height > 0);
		CHECK(api->follow && api->step && api->pixels && api->touch &&
			  api->muted && api->explained);
	}
}

std::vector<uint8_t> pixels(const airgrep_display_api& display) {
	const uint8_t* first = display.pixels();
	return {first,
			first + static_cast<size_t>(display.width) * display.height * 4};
}

size_t differing(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
	size_t count = 0;
	for (size_t at = 0; at < a.size(); at += 4) {
		count += std::memcmp(&a[at], &b[at], 3) != 0;
	}
	return count;
}

display::Settings stored_settings() {
	auto flight = device::open_flight();
	auto bytes = device::Session(*flight).read_settings(display::kSettingsKey);
	display::Settings settings;
	if (bytes.size() != sizeof settings) {
		throw std::runtime_error("Wrong settings size");
	}
	std::memcpy(&settings, bytes.data(), sizeof settings);
	return settings;
}

void settings_edited_during_read_preserve_other_device_values() {
	using remote::view::SettingsEdits;
	display::Settings local, saved;
	local.theme = display::Theme::dark;
	saved.radar_range_index = 4;
	saved.conflict_indicator = 1;
	saved.tracks = 0;
	SettingsEdits edits;
	edits.changed.set(SettingsEdits::theme);
	edits.changed.set(SettingsEdits::tracks);
	const auto merged = edits.merge(saved, local);
	CHECK(merged.theme == display::Theme::dark);
	CHECK_EQUAL(1, merged.tracks);
	CHECK_EQUAL(4, merged.radar_range_index);
	CHECK_EQUAL(1, merged.conflict_indicator);
}

void configure_device(const display::Settings& settings) {
	auto flight = device::open_flight();
	device::Session session(*flight);
	session.erase_settings();
	session.write_settings(
		display::kSettingsKey,
		{reinterpret_cast<const uint8_t*>(&settings), sizeof settings});
}

template <typename Predicate>
bool eventually(Predicate reached) {
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	while (!reached()) {
		if (std::chrono::steady_clock::now() >= deadline) {
			return false;
		}
		std::this_thread::sleep_for(1ms);
	}
	return true;
}

bool shows_startup_logo(const airgrep_display_api& display) {
	const uint8_t* pill =
		display.pixels() +
		4 * (126 * static_cast<ptrdiff_t>(display.width) + 436);
	return pill[1] > pill[0] + 60 && pill[1] > pill[2] + 60;
}

bool shows_guidance_bands(const char* explanation) {
	const char* counts = std::strstr(explanation, "bands: track ");
	unsigned track = 0, altitude = 0, speed = 0;
	return counts &&
		   std::sscanf(counts, "bands: track %u  altitude %u  speed %u", &track,
					   &altitude, &speed) == 3 &&
		   (track || altitude || speed);
}

constexpr char kNoBands[] = "bands: track 0  altitude 0  speed 0";

void shows_the_device() {
	const auto* display =
		get_display(AIRGREP_DISPLAY_ABI, sizeof(airgrep_display_api));
	CHECK(display != nullptr);
	if (!display) {
		return;
	}
	CHECK_EQUAL(800, display->width);
	CHECK_EQUAL(480, display->height);

	display::Settings settings;
	settings.radar_range_index = 3;
	settings.conflict_indicator = 1;
	configure_device(settings);

	airgrep_connection* connection = airgrep_connect(AIRGREP_EMULATED);
	airgrep_set_day(connection, kDay);
	airgrep_status status;
	CHECK(eventually([&] {
		airgrep_status_of(connection, &status);
		return status.phase == AIRGREP_CONNECTED;
	}));
	CHECK_EQUAL(static_cast<int>(AIRGREP_CONNECTED), status.phase);
	CHECK(status.readable);
	CHECK_EQUAL(std::string("Emulated"), status.board);
	CHECK_EQUAL(std::string("ON"), status.mode);
	CHECK_EQUAL(std::string("OK (0x0)"), status.health_names);

	auto run = [&](int ticks) {
		for (int i = 0; i < ticks; i++) {
			display->follow(connection, AIRGREP_EMULATED);
			display->step(16);
		}
	};
	CHECK(eventually([&] {
		run(1);
		return shows_startup_logo(*display);
	}));
	const std::vector<uint8_t> startup = pixels(*display);
	airgrep_watch(connection, 100);
	CHECK(eventually([&] {
		run(1);
		airgrep_status_of(connection, &status);
		return status.pictures_read > 0 && !shows_startup_logo(*display) &&
			   std::strstr(display->explained(), "of 10 reports");
	}));
	CHECK(differing(startup, pixels(*display)) > 20000);
	CHECK(!display->muted());
	CHECK(std::strstr(display->explained(), "range 16 NM"));
	CHECK(std::strstr(display->explained(), "conflict indicator on"));
	CHECK(eventually([&] {
		run(1);
		return shows_guidance_bands(display->explained());
	}));

	auto tap = [&](int x, int y) {
		display->touch(x, y, true);
		run(6);
		display->touch(x, y, false);
		run(60);
	};
	const auto open_system_page = [&] { tap(200, 30); };
	const auto select_dark_theme = [&] { tap(735, 453); };
	const auto select_simple_symbols = [&] { tap(745, 221); };
	const auto scroll_to_symbol_style = [&] {
		display->touch(200, 460, true);
		run(6);
		for (int y = 440; y >= 70; y -= 10) {
			display->touch(200, y, true);
			run(1);
		}
		run(30);
		display->touch(200, 70, false);
		run(60);
	};

	open_system_page();
	const std::vector<uint8_t> light_system = pixels(*display);
	select_dark_theme();
	CHECK(eventually([&] {
		run(1);
		settings = stored_settings();
		return settings.theme == display::Theme::dark;
	}));
	CHECK(differing(light_system, pixels(*display)) > 20000);
	CHECK_EQUAL(3, settings.radar_range_index);
	CHECK(settings.symbol_style == display::SymbolStyle::planes);
	scroll_to_symbol_style();
	select_simple_symbols();
	CHECK(eventually([&] {
		run(1);
		settings = stored_settings();
		return settings.symbol_style == display::SymbolStyle::simple;
	}));
	CHECK(settings.theme == display::Theme::dark);

	// Unfollowing stops background conflict work and following again resumes.
	display->follow(nullptr, nullptr);
	CHECK(eventually([&] {
		run(1);
		return shows_guidance_bands(display->explained());
	}));

	display->follow(nullptr, nullptr);
	airgrep_watch(connection, 0);
	airgrep_disconnect(connection);
}

void shows_no_bands_without_the_switch() {
	configure_device({});
	const auto* display =
		get_display(AIRGREP_DISPLAY_ABI, sizeof(airgrep_display_api));
	CHECK(display != nullptr);
	if (!display) {
		return;
	}
	airgrep_connection* connection = airgrep_connect(AIRGREP_EMULATED);
	airgrep_set_day(connection, kDay);
	airgrep_watch(connection, 20);
	airgrep_status status;
	const auto deadline = std::chrono::steady_clock::now() + 2s;
	do {
		display->follow(connection, AIRGREP_EMULATED);
		display->step(16);
		airgrep_status_of(connection, &status);
		std::this_thread::sleep_for(1ms);
	} while (status.pictures_read < 5 &&
			 std::chrono::steady_clock::now() < deadline);
	CHECK(status.pictures_read >= 5);
	for (int i = 0; i < 30; i++) {
		display->follow(connection, AIRGREP_EMULATED);
		display->step(16);
		std::this_thread::sleep_for(1ms);
	}
	const std::string explained = display->explained();
	CHECK(explained.find("conflict indicator off") != std::string::npos);
	CHECK(explained.find(kNoBands) != std::string::npos);
	airgrep_watch(connection, 0);
	airgrep_disconnect(connection);
}
}

int main(int argc, char* const* argv) {
	const bool abi_only = argc == 3 && std::strcmp(argv[2], "--abi") == 0;
	if (argc != 2 && !abi_only) {
		std::printf("usage: %s display-library [--abi]\n", argv[0]);
		return 2;
	}
	std::string load_error;
	get_display = display_loader::load_getter(argv[1], load_error);
	if (!get_display) {
		(void)std::fprintf(stderr, "%s\n", load_error.c_str());
		return 1;
	}
	return abi_only
			   ? check::run({TEST(negotiates_the_display_abi)})
			   : check::run(
					 {TEST(
						  settings_edited_during_read_preserve_other_device_values),
					  TEST(shows_the_device),
					  TEST(shows_no_bands_without_the_switch)});
}

#include <chrono>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "app/airgrep.h"
#include "app/connection.hpp"
#include "app/display.h"
#include "display_loader.hpp"
#include "gestures.hpp"
#include "png.hpp"

namespace {
using namespace std::chrono_literals;
using gestures::Gesture, gestures::kGestureMs;

// The device of docs/ui-design/sketch.
constexpr char kSerial[] = "a1b2c3d4e5f6";
constexpr uint32_t kStepMs = 10;
// Display time for the screen to come to rest: transitions and the radar. It
// ends in the bright half of the alert blink.
constexpr uint32_t kSettleMs = 2500, kSettleAfterGesturesMs = 300;
constexpr int kPicturesBeforeSettling = 3;
// The conflict engine has answered when what the display explains holds over
// this many pictures.
constexpr uint64_t kPicturesUnchanged = 10;

// The day of the time of day the fixtures say, in winter as the design.
constexpr std::chrono::sys_days kDay =
	std::chrono::year{2026} / std::chrono::January / 15;

int fail(const char* what) {
	(void)std::fprintf(stderr, "%s\n", what);
	return 1;
}

bool write(const char* path, const airgrep_display_api& display) {
	const size_t count = static_cast<size_t>(display.width) *
						 static_cast<size_t>(display.height);
	const uint8_t* bgrx = display.pixels();
	std::vector<uint16_t> rgb565(count);
	for (size_t at = 0; at < count; at++) {
		const unsigned blue = bgrx[at * 4], green = bgrx[at * 4 + 1],
					   red = bgrx[at * 4 + 2];
		rgb565[at] = static_cast<uint16_t>(red >> 3U << 11U |
										   green >> 2U << 5U | blue >> 3U);
	}
	return png::write(path, rgb565, display.width, display.height);
}
}

int main(int argc, char* const* argv) {
	bool startup = false;
	const char* frames_path = nullptr;
	std::chrono::sys_days day = kDay;
	std::vector<Gesture> steps;
	bool understood = argc >= 4;
	for (int i = 4; understood && i < argc; i++) {
		const std::string_view option = argv[i];
		if (option == "--startup") {
			startup = true;
		} else if (option == "--frames" && i + 1 < argc) {
			frames_path = argv[++i];
		} else if (option == "--date" && i + 1 < argc) {
			i++;
			int year = 0;
			unsigned month = 0, of_month = 0;
			understood =
				std::sscanf(argv[i], "%d-%u-%u", &year, &month, &of_month) == 3;
			day = std::chrono::year{year} / std::chrono::month{month} /
				  std::chrono::day{of_month};
		} else if (option == "--do" && i + 1 < argc) {
			i++;
			understood = gestures::parse(argv[i], steps.emplace_back());
		} else {
			understood = false;
		}
	}
	if (!understood) {
		(void)std::fprintf(
			stderr,
			"usage: %s display-library fixture png [--startup] [--date "
			"YYYY-MM-DD] [--frames bgr0-file] [--do gesture]...\n"
			"Draws what a display shows of a fixture into a PNG file and "
			"prints what the display explains.\n"
			"  fixture    its name and the display settings, in one word "
			"list: \"busy tracks range=16\"\n"
			"  --startup  the screen before the first picture\n"
			"  --date     the day of the time the device says, 2026-01-15 "
			"without it\n"
			"  --frames   record gestures at 25 fps, advancing the fixture "
			"once per simulated second\n"
			"  --do       tap:X,Y  drag:X,Y,X,Y  hold:X,Y,X,Y  wait:MS  "
			"gnss:FIXTURE  device:MS, one after the other.\n"
			"             gnss hands the device the GNSS data of that "
			"fixture\n"
			"             device lets the device run on that long, it stays "
			"at its scene otherwise\n",
			argv[0]);
		return 2;
	}
	std::string load_error;
	const airgrep_display_api* display =
		display_loader::load_api(argv[1], load_error);
	if (!display) {
		return fail(load_error.c_str());
	}

	struct ConnectionGuard {
		void operator()(airgrep_connection* c) const {
			airgrep_watch(c, 0);
			airgrep_disconnect(c);
		}
	};
	const std::unique_ptr<airgrep_connection, ConnectionGuard> owned_connection(
		airgrep_connect_fixture(argv[2]));
	airgrep_connection* connection = owned_connection.get();
	airgrep_set_day(connection, day.time_since_epoch().count());
	airgrep_watch(connection, startup ? 0 : 20);
	const auto pass = [&](uint32_t display_ms) {
		display->follow(connection, kSerial);
		display->step(display_ms);
	};
	airgrep_status status;
	bool shown = false;
	// The conflict engine answers on a thread of its own. Waiting for it and
	// for the device takes no display time, so every run shows the same
	// moment.
	const auto rest = [&] {
		std::string explained;
		uint64_t explained_at = 0;
		const auto deadline = std::chrono::steady_clock::now() + 5s;
		do {
			pass(0);
			std::this_thread::sleep_for(1ms);
			airgrep_status_of(connection, &status);
			if (explained != display->explained()) {
				explained = display->explained();
				explained_at = status.pictures_read;
			}
			shown = startup ? status.phase == AIRGREP_CONNECTED
							: status.pictures_read >= kPicturesBeforeSettling &&
								  status.pictures_read >=
									  explained_at + kPicturesUnchanged;
		} while (!shown && status.phase != AIRGREP_ENDED &&
				 std::chrono::steady_clock::now() < deadline);
	};
	rest();
	for (uint32_t at = 0; shown && at < kSettleMs; at += kStepMs) {
		pass(kStepMs);
	}
	std::ofstream frames;
	if (frames_path) {
		frames.open(frames_path, std::ios::binary);
		if (!frames) {
			return fail("Cannot open the raw video file");
		}
	}
	uint32_t recorded_ms = 0;
	for (const Gesture& gesture : steps) {
		if (shown && gesture.kind == Gesture::gnss) {
			connection->connection.update_gnss(gesture.reported);
			rest();
			continue;
		}
		if (shown && gesture.kind == Gesture::device) {
			airgrep_fixture_pass(connection, gesture.wait_ms);
			rest();
			continue;
		}
		const uint32_t lasts =
			gesture.kind == Gesture::wait ? gesture.wait_ms : kGestureMs;
		for (uint32_t at = 0; shown && at < lasts; at += kStepMs) {
			// Sample before publishing inputs at the same timestamp.
			if (frames_path && recorded_ms % 40 == 0) {
				frames.write(
					reinterpret_cast<const char*>(display->pixels()),
					std::streamsize{display->width} * display->height * 4);
			}
			if (frames_path && recorded_ms && recorded_ms % 1000 == 0) {
				airgrep_fixture_pass(connection, 1000);
				rest();
			}
			gestures::touch(*display, gesture, at);
			pass(kStepMs);
			recorded_ms += kStepMs;
		}
	}
	if (frames_path) {
		frames.close();
		if (!frames) {
			return fail("Cannot write the raw video file");
		}
	}
	if (shown && !steps.empty()) {
		rest();
		for (uint32_t at = 0; shown && at < kSettleAfterGesturesMs;
			 at += kStepMs) {
			pass(kStepMs);
		}
	}
	const bool written = shown && write(argv[3], *display);
	(void)std::fputs(display->explained(), stdout);
	if (!shown) {
		return fail(status.error[0] ? status.error : "The device sent nothing");
	}
	return written ? 0 : fail("Cannot write the PNG file");
}

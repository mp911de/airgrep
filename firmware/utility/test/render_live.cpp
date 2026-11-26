// Record the real Remote Display poller and conflict worker during the Demo
// head-on encounter. The frame file is native 800x480 BGRX at 60 Hz.
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "800x480-touch/display_settings.hpp"
#include "app/connection.hpp"
#include "app/display.h"
#include "device/emulator.hpp"
#include "display_loader.hpp"

namespace {
using Clock = std::chrono::steady_clock;

class Encounter : public device::Transport {
	device::EmulatedDevice device_ = device::make_booted_service_device();
	device::Emulator emulator_{device_};
	const Clock::time_point start_ = Clock::now();

   public:
	Encounter() {
		display::Settings settings;
		settings.conflict_indicator = true;
		settings.radar_range_index = 1;
		const auto* bytes = reinterpret_cast<const uint8_t*>(&settings);
		device_.flash->values[display::kSettingsKey] = {
			bytes, bytes + sizeof settings};
	}
	void reset_session() override { emulator_.reset_session(); }
	void write(std::span<const uint8_t> data) override {
		const std::chrono::duration<double> elapsed = Clock::now() - start_;
		device::fly(device_, 66 + elapsed.count());
		emulator_.write(data);
	}
	bool read(std::vector<uint8_t>& bytes, size_t size,
			  std::chrono::milliseconds timeout) override {
		return emulator_.read(bytes, size, timeout);
	}
};

struct Coverage {
	unsigned warning = 0, recovery = 0;
};

// Inspect the display buffer before movie compression. Limit sampling to the
// radar viewport to exclude red and green table badges and status icons.
Coverage coverage(const uint8_t* pixels) {
	Coverage result;
	for (size_t y = 72; y < 460; ++y) {
		for (size_t x = 22; x < 384; ++x) {
			const auto* bgr = pixels + 4 * (800 * y + x);
			const int b = bgr[0], g = bgr[1], r = bgr[2];
			result.warning += r > 232 && r > g + 8 && g > 185 && g < 245 &&
							  std::abs(g - b) <= 6;
			result.recovery += g > 125 && g > r + 45 && g > b + 40;
		}
	}
	return result;
}
}

int main(int argc, char** argv) {
	if (argc != 4) {
		std::cerr << "Usage: " << argv[0]
				  << " DISPLAY-DYLIB COUNTS.csv FRAMES.bgrx\n";
		return 1;
	}
	std::string load_error;
	const auto* api = display_loader::load_api(argv[1], load_error);
	if (!api) {
		std::cerr << load_error << '\n';
		return 1;
	}
	std::ofstream counts(argv[2]), video(argv[3], std::ios::binary);
	if (!counts || !video || api->width != 800 || api->height != 480) {
		std::cerr << "Cannot open output or 800x480 display\n";
		return 1;
	}
	counts << "frame,clock_s,track,altitude,speed,red,green\n";
	airgrep_connection connection([] { return std::make_unique<Encounter>(); });
	airgrep_set_day(&connection, 20468);
	airgrep_watch(&connection, 100);
	const auto start = Clock::now();
	int frames = 0;
	for (int tick = 0; tick < 1100 && frames < 720; ++tick) {
		std::this_thread::sleep_until(
			start + std::chrono::microseconds(tick * 1'000'000LL / 60));
		api->follow(&connection, "a1b2c3d4e5f6");
		api->step(16 + (tick % 3 != 0));
		auto* pixels = api->pixels();
		if (!pixels) {
			continue;
		}
		unsigned track = 0, altitude = 0, speed = 0;
		const auto* bands = std::strstr(api->explained(), "bands: track ");
		if (bands) {
			(void)std::sscanf(bands, "bands: track %u  altitude %u  speed %u",
							  &track, &altitude, &speed);
		}
		const auto observed = coverage(pixels);
		if (!frames &&
			!(track && observed.warning > 1000 && observed.recovery > 100)) {
			continue;
		}
		if (!video.write(reinterpret_cast<const char*>(pixels),
						 4L * 800 * 480)) {
			return 1;
		}
		const std::chrono::duration<double> elapsed = Clock::now() - start;
		counts << frames << ',' << elapsed.count() << ',' << track << ','
			   << altitude << ',' << speed << ',' << observed.warning << ','
			   << observed.recovery << '\n';
		++frames;
	}
	return frames == 720 && video.flush() && counts.flush() ? 0 : 1;
}

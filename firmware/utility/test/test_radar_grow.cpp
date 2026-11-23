#include <array>
#include <cstdint>

#include "check.hpp"
#include "widgets/radar/radar_grow.h"

namespace {
void one_pixel() {
	const uint8_t from[1] = {200};
	std::array<uint8_t, 25> to{};
	to.fill(7);
	radar_grow(from, 1, 1, 1, to.data(), 5, 2);
	const uint8_t disc[25] = {0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1,
							  1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0};
	for (size_t i = 0; i < to.size(); i++) {
		CHECK_EQUAL(disc[i] ? 200 : 0, to[i]);
	}
}

void strongest() {
	constexpr uint8_t untouched_padding = 7;
	const uint8_t from[4] = {10, 250, 99, 99};
	std::array<uint8_t, 12> to{};
	to.fill(untouched_padding);
	radar_grow(from, 2, 1, 4, to.data(), 6, 1);
	const uint8_t grown[12] = {
		10, 250, 250, 250, untouched_padding, untouched_padding,
		10, 250, 250, 250, untouched_padding, untouched_padding};
	for (size_t i = 0; i < 12; i++) {
		CHECK_EQUAL(grown[i], to[i]);
	}
}
}

int main() {
	return check::run({
		TEST(one_pixel),
		TEST(strongest),
	});
}

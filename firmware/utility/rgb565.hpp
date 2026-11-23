#pragma once

#include <cstdint>

namespace rgb565 {
constexpr uint8_t expand5(unsigned channel) {
	return (channel * 255 + 15) / 31;
}
constexpr uint8_t expand6(unsigned channel) {
	return (channel * 255 + 31) / 63;
}
}

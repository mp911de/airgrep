#pragma once

#include <cstdint>
#include <vector>

namespace png {
// Writes RGB565 pixels as an 8-bit RGB PNG. False if the file cannot be
// written.
bool write(const char* path, const std::vector<uint16_t>& pixels, int32_t width,
		   int32_t height);
}

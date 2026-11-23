#include "png.hpp"

#include <zlib.h>

#include <fstream>

#include "rgb565.hpp"

namespace png {
namespace {
constexpr uint8_t kFilterNone = 0, kBitDepth = 8, kTruecolor = 2;
void put32(std::vector<uint8_t>& out, uint32_t value) {
	for (const unsigned shift : {24U, 16U, 8U, 0U}) {
		out.push_back(static_cast<uint8_t>(value >> shift));
	}
}

void chunk(std::vector<uint8_t>& out, const char* type,
		   const std::vector<uint8_t>& data) {
	put32(out, static_cast<uint32_t>(data.size()));
	const size_t start = out.size();
	out.insert(out.end(), type, type + 4);
	out.insert(out.end(), data.begin(), data.end());
	put32(out,
		  static_cast<uint32_t>(crc32(0, out.data() + start,
									  static_cast<uInt>(out.size() - start))));
}
}

bool write(const char* path, const std::vector<uint16_t>& pixels, int32_t width,
		   int32_t height) {
	std::vector<uint8_t> rows;
	rows.reserve((static_cast<size_t>(width) * 3 + 1) *
				 static_cast<size_t>(height));
	for (int32_t y = 0; y < height; y++) {
		rows.push_back(kFilterNone);
		for (int32_t x = 0; x < width; x++) {
			const unsigned pixel = pixels[y * width + x];
			rows.push_back(rgb565::expand5(pixel >> 11U));
			rows.push_back(rgb565::expand6(pixel >> 5U & 63U));
			rows.push_back(rgb565::expand5(pixel & 31U));
		}
	}
	uLongf size = compressBound(rows.size());
	std::vector<uint8_t> packed(size);
	if (compress(packed.data(), &size, rows.data(), rows.size()) != Z_OK) {
		return false;
	}
	packed.resize(size);

	std::vector<uint8_t> out = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	std::vector<uint8_t> header;
	put32(header, width);
	put32(header, height);
	header.insert(header.end(), {kBitDepth, kTruecolor, 0, 0, 0});
	chunk(out, "IHDR", header);
	chunk(out, "IDAT", packed);
	chunk(out, "IEND", {});

	std::ofstream file(path, std::ios::binary);
	file.write(reinterpret_cast<const char*>(out.data()),
			   static_cast<std::streamsize>(out.size()));
	file.close();
	return file.good();
}
}

#pragma once

#include <cstdint>

namespace expander {
inline constexpr uint8_t kMutePin = 1 << 0, kWifiPin = 1 << 1,
						 kPowerPin = 1 << 7;

struct Input {
	bool high[8];
	bool mute, wifi, power;
};

Input decode_input(uint8_t raw);

struct Output {
	bool high[8];
};

Output decode_output(uint8_t raw);

struct Polarity {
	bool inverted[8];
};

Polarity decode_polarity(uint8_t raw);

struct Configuration {
	bool input[8];
};

Configuration decode_configuration(uint8_t raw);
}  // namespace expander

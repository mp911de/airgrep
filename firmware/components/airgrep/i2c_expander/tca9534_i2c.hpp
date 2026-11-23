#pragma once

#include <cstdint>

#include "device.h"

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

struct StateData {
	Status status = Status::unknown;
	uint8_t raw_input = 0, raw_output = 0, raw_polarity = 0,
			raw_configuration = 0;
	Input input{};
	Output output{};
	Polarity polarity{};
	Configuration configuration{};
	int64_t updated_us = 0, error_us = 0;
	EspErrorCode last_error = 0;
	bool identity_known = false, configured = false;
};
}

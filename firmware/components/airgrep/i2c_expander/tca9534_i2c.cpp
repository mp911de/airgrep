#include "tca9534_i2c.hpp"

namespace expander {
Configuration decode_configuration(uint8_t raw) {
	Configuration result{};
	for (unsigned pin = 0; pin < 8; pin++) {
		result.input[pin] = raw & (1u << pin);
	}
	return result;
}

Polarity decode_polarity(uint8_t raw) {
	Polarity result{};
	for (unsigned pin = 0; pin < 8; pin++) {
		result.inverted[pin] = raw & (1u << pin);
	}
	return result;
}

Output decode_output(uint8_t raw) {
	Output result{};
	for (unsigned pin = 0; pin < 8; pin++) {
		result.high[pin] = raw & (1u << pin);
	}
	return result;
}

Input decode_input(uint8_t raw) {
	Input result{};
	for (unsigned pin = 0; pin < 8; pin++) {
		result.high[pin] = raw & (1u << pin);
	}
	result.mute = raw & kMutePin;  // slider up pulls BUZZ_ENABLE high and mutes
	result.wifi = raw & kWifiPin;
	result.power = !(raw & kPowerPin);
	return result;
}
}

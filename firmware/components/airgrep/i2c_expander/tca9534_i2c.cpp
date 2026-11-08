// Copyright 2026 the original author or authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tca9534_i2c.hpp"

namespace expander {
	Configuration decode_configuration(uint8_t raw) {
		Configuration result{};
		for (unsigned pin = 0; pin < 8; pin++) result.input[pin] = raw & (1u << pin);
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
		result.mute = result.high[0]; // slider up pulls BUZZ_ENABLE high and mutes
		result.wifi = result.high[1];
		result.power = !result.high[7];
		return result;
	}
}

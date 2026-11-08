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

#include "fusb303_i2c.hpp"

namespace fusb {
	namespace {
		// Field layout, FUSB303B datasheet Tables 12, 16, 22 and 23.
		constexpr unsigned kIdentityVersionShift = 4; // DEVICE_ID VER_ID[3:0]
		constexpr uint8_t kIdentityRevisionMask = 0x0f; // DEVICE_ID REV_ID[3:0]
		constexpr uint8_t kControl1DebounceMask = 0x07; // CONTROL1 TCCDEB[2:0]
		constexpr unsigned kDebounceReserved = 7;
		constexpr unsigned kDebounceBaseMs = 120, kDebounceStepMs = 10;
		constexpr uint8_t kStatusAttach = 1 << 0;
		constexpr unsigned kStatusBcLvlShift = 1;
		constexpr uint8_t kStatusBcLvlMask = 0x03;
		constexpr uint8_t kStatusVbusOk = 1 << 3;
		constexpr unsigned kStatusOrientShift = 4;
		constexpr uint8_t kStatusOrientMask = 0x03;
		constexpr unsigned kBcLvlDefault = 1, kBcLvl1500mA = 2, kBcLvl3000mA = 3;
		constexpr unsigned kOrientCc1 = 1, kOrientCc2 = 2;
		constexpr uint8_t kTypeMask = 0x7f; // TYPE bit 7 reserved
		constexpr uint8_t kTypeAudio = 1 << 0;
		constexpr uint8_t kTypeAudioVbus = 1 << 1;
		constexpr uint8_t kTypeActiveCable = 1 << 2;
		constexpr uint8_t kTypeSource = 1 << 3;
		constexpr uint8_t kTypeSink = 1 << 4;
		constexpr uint8_t kTypeDebugSink = 1 << 5;
		constexpr uint8_t kTypeDebugSource = 1 << 6;
	}

	unsigned cc_limit(const PortStatus& status, Role role) {
		return status.attached && status.vbus && role == Role::sink &&
				(status.orientation == kOrientCc1 || status.orientation == kOrientCc2)
					? status.advertised_ma
					: 0;
	}

	Control decode_control1(uint8_t raw) {
		unsigned debounce = raw & kControl1DebounceMask;
		return {
			bool(raw & kControl1Enable),
			debounce == kDebounceReserved ? 0 : kDebounceBaseMs + debounce * kDebounceStepMs
		};
	}

	Role decode_type(uint8_t raw) {
		switch (raw & kTypeMask) {
		case 0: return Role::none;
		case kTypeAudio: return Role::audio;
		case kTypeAudioVbus: return Role::audio_vbus;
		case kTypeActiveCable: return Role::active_cable;
		case kTypeSource: return Role::source;
		case kTypeSink: return Role::sink;
		case kTypeDebugSink: return Role::debug_sink;
		case kTypeDebugSource: return Role::debug_source;
		default: return Role::invalid;
		}
	}

	PortStatus decode_status(uint8_t raw) {
		unsigned advertisement = (raw >> kStatusBcLvlShift) & kStatusBcLvlMask;
		unsigned advertised_ma = advertisement == kBcLvl1500mA ? 1500
			: advertisement == kBcLvl3000mA ? 3000 : 0;
		return {
			bool(raw & kStatusAttach), bool(raw & kStatusVbusOk),
			unsigned((raw >> kStatusOrientShift) & kStatusOrientMask),
			advertised_ma, advertisement == kBcLvlDefault
		};
	}

	Identity decode_identity(uint8_t raw) {
		return {unsigned(raw >> kIdentityVersionShift), unsigned(raw & kIdentityRevisionMask)};
	}
}

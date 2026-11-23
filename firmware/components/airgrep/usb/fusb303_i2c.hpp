#pragma once

#include <cstdint>

#include "device.h"

namespace fusb {
// Register fields shared by driver and decoder, FUSB303B datasheet Table 11.
static constexpr uint8_t kControl1Enable = 1 << 3;  // CONTROL1 ENABLE

struct Identity {
	unsigned version, revision;
};
Identity decode_identity(uint8_t raw);
struct PortStatus {
	bool attached, vbus;
	unsigned orientation, advertised_ma;
	bool default_current;
};
PortStatus decode_status(uint8_t raw);
enum class Role {
	none,
	sink,
	source,
	active_cable,
	audio,
	audio_vbus,
	debug_sink,
	debug_source,
	invalid
};
Role decode_type(uint8_t raw);
struct Control {
	bool enabled;
	unsigned debounce_ms;
};
Control decode_control1(uint8_t raw);
unsigned cc_limit(const PortStatus& status, Role role);
bool decode_remedy(uint8_t status1);
bool negotiating(const PortStatus& status, bool remedy);

struct StateData {
	Status status = Status::unknown;
	uint8_t raw_identity = 0, raw_status = 0, raw_status1 = 0, raw_type = 0,
			raw_control1 = 0;
	Identity identity{};
	PortStatus port{};
	Role role = Role::none;
	Control control{};
	unsigned cc_limit_ma = 0;
	int64_t updated_us = 0, error_us = 0;
	EspErrorCode last_error = 0;
	bool identity_known = false, configured = false, remedy = false;

	bool usable(int64_t now_us) const {
		return state_usable(updated_us, now_us);
	}
	bool negotiating() const { return fusb::negotiating(port, remedy); }
	// 3000, 1500, or 0 for default current or no attached powered sink.
	unsigned advertised_ma() const { return cc_limit_ma; }
};
}

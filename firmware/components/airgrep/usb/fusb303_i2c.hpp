#pragma once

#include <cstdint>

namespace fusb {
	// Register fields shared by driver and decoder, FUSB303B datasheet Table 11.
	static constexpr uint8_t kControl1Enable = 1 << 3; // CONTROL1 ENABLE

	struct Identity { unsigned version, revision; };
	Identity decode_identity(uint8_t raw);
	struct PortStatus { bool attached, vbus; unsigned orientation, advertised_ma; bool default_current; };
	PortStatus decode_status(uint8_t raw);
	enum class Role { none, sink, source, active_cable, audio, audio_vbus, debug_sink, debug_source, invalid };
	Role decode_type(uint8_t raw);
	struct Control { bool enabled; unsigned debounce_ms; };
	Control decode_control1(uint8_t raw);
	unsigned cc_limit(const PortStatus& status, Role role);
}

#include "hwtest.hpp"
#include <cstdio>
#include "i2c_expander/tca9534.hpp"
#include "charger/bq25798.hpp"
#include "battery/max17048.hpp"
#include "baro/barometer.hpp"
#include "usb/fusb303.hpp"

namespace hwtest {
	void report(const char* name, bool pass, const char* detail) {
		std::printf("HWTEST %s %s %s\n", name, pass ? "PASS" : "FAIL", detail);
	}

	void run() {
		const auto charger = charger::State::copy();
		char charger_detail[32];
		std::snprintf(charger_detail, sizeof charger_detail, "REG48 0x%02X", charger.raw[0x48]);
		report("charger_identity", charger.identity_known, charger_detail);
		const auto battery = battery::State::copy();
		char battery_detail[32];
		std::snprintf(battery_detail, sizeof battery_detail, "VERSION 0x%04X", battery.raw_version);
		report("battery_identity", battery.identity_known, battery_detail);
		const auto baro = barometer::State::copy();
		char baro_detail[32];
		std::snprintf(baro_detail, sizeof baro_detail, "CHIP_ID 0x%02X", baro.chip_id);
		report("barometer_identity", baro.identity_known, baro_detail);
		const auto fusb = fusb::State::copy();
		char fusb_detail[32];
		std::snprintf(fusb_detail, sizeof fusb_detail, "DEVICE_ID 0x%02X", fusb.raw_identity);
		report("fusb_identity", fusb.identity_known, fusb_detail);
		const auto exp = expander::State::copy();
		char detail[32];
		std::snprintf(detail, sizeof detail, "INPUT_PORT 0x%02X", exp.raw_input);
		report("expander_identity", exp.identity_known, detail);
	}
} // namespace hwtest

#include "hwtest.hpp"
#include <cstdio>
#include "i2c_bus.h"
#include "loop.hpp"

namespace hwtest {
	namespace {
		// Each check is one bounded I2C transaction, so it is one action.
		loop::Result probe(const char* name, uint8_t address) {
			char detail[8];
			std::snprintf(detail, sizeof detail, "0x%02x", address);
			report(name, i2c_bus_present(address), detail);
			return loop::Result::done;
		}
	} // namespace

	void report(const char* name, bool pass, const char* detail) {
		std::printf("HWTEST %s %s %s\n", name, pass ? "PASS" : "FAIL", detail);
	}

	void run() {
		loop::Handle h = loop::post("hwtest gauge", [](loop::Handle, loop::Value&) {
			probe("i2c_gauge", 0x36);
			if (!loop::post("hwtest usb", [](loop::Handle, loop::Value&) {
				return probe("i2c_usb_controller", 0x21);
			}).valid()) {
				report("i2c_usb_controller", false, "not submitted");
				return loop::Result::error;
			}
			return loop::Result::done;
		});
		if (!h.valid()) {
			report("i2c_gauge", false, "not submitted");
		}
	}
} // namespace hwtest

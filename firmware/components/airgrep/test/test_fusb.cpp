#include "usb/fusb303_i2c.hpp"
#include "unity.h"

#ifdef ESP_PLATFORM
#define CASE(name, description) TEST_CASE(description, "[fusb]")
#else
#define CASE(name, description) static void name()
void setUp() {}
void tearDown() {}
#endif

CASE(identity, "FUSB device ID separates version and revision") {
	const auto id = fusb::decode_identity(0x12);
	TEST_ASSERT_EQUAL_UINT(1, id.version);
	TEST_ASSERT_EQUAL_UINT(2, id.revision);
}

CASE(status, "FUSB STATUS decodes attach orientation VBUS and CC advertisement") {
	const auto cc1 = fusb::decode_status(0x1d);
	TEST_ASSERT_TRUE(cc1.attached);
	TEST_ASSERT_TRUE(cc1.vbus);
	TEST_ASSERT_EQUAL_UINT(1, cc1.orientation);
	TEST_ASSERT_EQUAL_UINT(1500, cc1.advertised_ma);
	const auto cc2 = fusb::decode_status(0x2f);
	TEST_ASSERT_EQUAL_UINT(2, cc2.orientation);
	TEST_ASSERT_EQUAL_UINT(3000, cc2.advertised_ma);
	const auto idle = fusb::decode_status(0x40);
	TEST_ASSERT_FALSE(idle.attached);
	TEST_ASSERT_FALSE(idle.vbus);
	TEST_ASSERT_EQUAL_UINT(0, idle.advertised_ma);
	TEST_ASSERT_EQUAL_UINT(0, fusb::decode_status(0x03).advertised_ma);
	for (unsigned raw = 0; raw < 256; raw++)
		TEST_ASSERT_EQUAL((raw / 2) % 4 == 1, fusb::decode_status(raw).default_current);
	TEST_ASSERT_EQUAL_UINT(3, fusb::decode_status(0x30).orientation);
}

CASE(type, "FUSB TYPE distinguishes sink source accessories and invalid combinations") {
	TEST_ASSERT_TRUE(fusb::decode_type(0x10) == fusb::Role::sink);
	TEST_ASSERT_TRUE(fusb::decode_type(0x08) == fusb::Role::source);
	TEST_ASSERT_TRUE(fusb::decode_type(0x04) == fusb::Role::active_cable);
	TEST_ASSERT_TRUE(fusb::decode_type(0x02) == fusb::Role::audio_vbus);
	TEST_ASSERT_TRUE(fusb::decode_type(0x01) == fusb::Role::audio);
	TEST_ASSERT_TRUE(fusb::decode_type(0x20) == fusb::Role::debug_sink);
	TEST_ASSERT_TRUE(fusb::decode_type(0x40) == fusb::Role::debug_source);
	TEST_ASSERT_TRUE(fusb::decode_type(0) == fusb::Role::none);
	TEST_ASSERT_TRUE(fusb::decode_type(0x18) == fusb::Role::invalid);
}

CASE(control, "FUSB CONTROL1 decodes enable and debounce while ignoring unrelated bits") {
	const auto por = fusb::decode_control1(0x23);
	TEST_ASSERT_FALSE(por.enabled);
	TEST_ASSERT_EQUAL_UINT(150, por.debounce_ms);
	const auto enabled = fusb::decode_control1(0xae);
	TEST_ASSERT_TRUE(enabled.enabled);
	TEST_ASSERT_EQUAL_UINT(180, enabled.debounce_ms);
	TEST_ASSERT_EQUAL_UINT(0, fusb::decode_control1(0x07).debounce_ms);
}

CASE(limit, "CC limit requires an attached powered sink with resolved orientation") {
	TEST_ASSERT_EQUAL_UINT(1500, fusb::cc_limit(fusb::decode_status(0x1d), fusb::Role::sink));
	TEST_ASSERT_EQUAL_UINT(3000, fusb::cc_limit(fusb::decode_status(0x2f), fusb::Role::sink));
	TEST_ASSERT_EQUAL_UINT(0, fusb::cc_limit(fusb::decode_status(0x1b), fusb::Role::sink));
	TEST_ASSERT_EQUAL_UINT(0, fusb::cc_limit(fusb::decode_status(0x1c), fusb::Role::sink));
	TEST_ASSERT_EQUAL_UINT(0, fusb::cc_limit(fusb::decode_status(0x15), fusb::Role::sink));
	TEST_ASSERT_EQUAL_UINT(0, fusb::cc_limit(fusb::decode_status(0x3f), fusb::Role::sink));
	TEST_ASSERT_EQUAL_UINT(0, fusb::cc_limit(fusb::decode_status(0x1d), fusb::Role::source));
}

#ifndef ESP_PLATFORM
int main() {
	UNITY_BEGIN();
	RUN_TEST(identity);
	RUN_TEST(status);
	RUN_TEST(type);
	RUN_TEST(control);
	RUN_TEST(limit);
	return UNITY_END();
}
#endif

#include "charger/bq25798.hpp"
#include "system/charge_control.hpp"
#include "unity.h"

TEST_CASE("Changing to MANUAL clears the published AUTO decision", "[charge]") {
	using charger::ControlMode;
	TEST_ASSERT_EQUAL(ESP_OK,
					  charge_control::set_charger_mode(ControlMode::automatic));
	charger::State::update([](charger::State& state) {
		state.target.target_available = true;
		state.target.stop = charger::ChargeStop::hot;
	});
	TEST_ASSERT_EQUAL(ESP_OK,
					  charge_control::set_charger_mode(ControlMode::manual));
	const auto target = charger::State::copy().target;
	TEST_ASSERT_EQUAL(ESP_OK,
					  charge_control::set_charger_mode(ControlMode::automatic));
	TEST_ASSERT_TRUE(target.mode == ControlMode::manual);
	TEST_ASSERT_FALSE(target.target_available);
	TEST_ASSERT_TRUE(target.stop == charger::ChargeStop::none);
}

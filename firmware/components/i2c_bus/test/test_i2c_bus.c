#include "i2c_bus.h"
#include "unity.h"

TEST_CASE (
"Carrier parts answer on the bus"
,
"[i2c][hw]"
)
{
    TEST_ASSERT_EQUAL(ESP_OK, i2c_bus_init());
    i2c_bus_log_scan();
    TEST_ASSERT_TRUE_MESSAGE(i2c_bus_present(0x36), "MAX17048 gauge");
    TEST_ASSERT_TRUE_MESSAGE(i2c_bus_present(0x21), "FUSB303B");
}

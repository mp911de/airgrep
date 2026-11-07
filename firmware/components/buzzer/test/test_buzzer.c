#include "buzzer.h"
#include "unity.h"

TEST_CASE (
"Buzzer beeps for 200 ms"
,
"[buzzer][hw]"
)
{
    TEST_ASSERT_EQUAL(ESP_OK, buzzer_init());
    buzzer_set_mute(false);
    buzzer_beep_ms(200);
}

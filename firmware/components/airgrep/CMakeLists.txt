idf_component_register(SRCS "adsbee_link.cpp" "decoder.c" "buzzer.c" "buzzer_beep.cpp" "i2c_bus.c"
        INCLUDE_DIRS "include"
        REQUIRES loop esp_driver_i2c
        PRIV_REQUIRES esp_driver_uart esp_driver_ledc)

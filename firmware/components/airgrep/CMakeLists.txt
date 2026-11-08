idf_component_register(SRCS "adsbee/adsbee_link.cpp" "adsbee/adsbee_decoder.c" "buzzer/buzzer.c" "buzzer/buzzer_beep.cpp" "i2c_bus.c"
        "i2c_expander/tca9534.cpp" "i2c_expander/tca9534_i2c.cpp" "charger/bq25798.cpp" "charger/bq25798_i2c.cpp"
        "battery/max17048.cpp" "battery/max17048_i2c.cpp" "baro/barometer.cpp" "baro/bmp580_i2c.cpp"
        "usb/fusb303.cpp" "usb/fusb303_i2c.cpp" "buzzer/buzzer_pattern.c" "adsbee/adsbee_protocol.cpp" "adsbee/adsbee_time.cpp" "adsbee/adsbee_clock.cpp"
        "settings/settings.cpp" "settings/settings_model.cpp" "tz/tz.cpp" "tz/tz_lookup.c" "wifi/wifi_ap.cpp" "wifi/wifi_protocol.cpp"
        "traffic/traffic.cpp" "traffic/traffic_protocol.cpp" "traffic/traffic_table.cpp"
        INCLUDE_DIRS "."
        REQUIRES loop esp_driver_i2c nvs_flash esp_partition
        PRIV_REQUIRES esp_driver_uart esp_driver_ledc esp_driver_gpio esp_timer)

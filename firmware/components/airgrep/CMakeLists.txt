set(service_sources "adsbee/gnss_ingestion.cpp"
    "tz/tz.cpp" "tz/tz_lookup.c"
    "navigation/great_circle.cpp" "system/device_info.c" "adsbee/adsbee_protocol.cpp"
    "settings/settings.cpp" "settings/settings_model.cpp" "traffic/traffic.cpp"
    "traffic/traffic_table.cpp" "traffic/traffic_protocol.cpp" "events/events.cpp")
if(IDF_TARGET STREQUAL "esp32p4")
    idf_component_register(SRCS ${service_sources} INCLUDE_DIRS "." REQUIRES loop nvs_flash esp_driver_i2c
            PRIV_REQUIRES efuse esp_app_format esp_pm esp_psram spi_flash esp_timer esp_partition)
else()
    idf_component_register(SRCS ${service_sources} "adsbee/adsbee_link.cpp" "adsbee/adsbee_decoder.c" "buzzer/buzzer.c" "buzzer/buzzer_beep.cpp" "i2c_bus.c"
            "i2c_expander/tca9534.cpp" "i2c_expander/tca9534_i2c.cpp" "charger/bq25798.cpp" "charger/bq25798_i2c.cpp"
            "battery/max17048.cpp" "battery/max17048_i2c.cpp" "baro/barometer.cpp" "baro/bmp580_i2c.cpp"
            "usb/fusb303.cpp" "usb/fusb303_i2c.cpp" "buzzer/buzzer_pattern.c" "adsbee/adsbee_time.cpp" "adsbee/adsbee_clock.cpp"
            "wifi/wifi_ap.cpp" "wifi/wifi_protocol.cpp"
            "system/system.cpp" "system/power_mode.c"
            "system/charge_control.cpp" "system/charge_rules.cpp" "system/health.cpp"
            INCLUDE_DIRS "."
            REQUIRES loop esp_driver_i2c nvs_flash esp_partition
            PRIV_REQUIRES esp_driver_uart esp_driver_ledc esp_driver_gpio esp_timer esp_pm
            esp_app_format efuse spi_flash esp_psram)
endif()
# Every image declares its board, also one whose code never asks for it: the Utility and airgrep.py flash
# check it before they write the image.
target_link_options(${COMPONENT_LIB} INTERFACE "LINKER:-u,device_board_name")

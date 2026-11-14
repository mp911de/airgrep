idf_build_get_property(test AIRGREP_TEST)
if (test)
    idf_component_register(SRCS "test_main.c" "log/log_ring.c" "test/test_log_ring.c"
            "log/log_session.c" "test/test_log_session.c" "system/task_stats.c" "test/test_task_stats.c"
            "usb/usb_protocol.cpp" "test/test_usb_protocol.cpp"
            "usb/usb_commands.cpp" "test/test_usb_commands.cpp"
            INCLUDE_DIRS "." "log" "commands" "system" PRIV_REQUIRES unity airgrep WHOLE_ARCHIVE)
else ()
    set(usb_service_sources)
    if(CONFIG_AIRGREP_USB_SERVICE_COMMANDS)
        list(APPEND usb_service_sources "usb/usb_commands.cpp" "usb/usb_components.cpp")
    endif()
    idf_component_register(SRCS "main.cpp" "console.c" "banner.c" "commands/cmd_uptime.cpp"
            "commands/cmd_reboot.c" "commands/cmd_logs.c" "log/log_ring.c" "log/log_session.c" "commands/cmd_exp.cpp"
            "commands/cmd_charger.cpp" "commands/cmd_battery_level.cpp" "commands/cmd_baro.cpp"
            "commands/cmd_fusb.cpp" "commands/cmd_beep.cpp" "commands/cmd_adsbee.cpp" "commands/cmd_gnss.cpp" "commands/cmd_clock.cpp" "commands/cmd_settings.cpp" "commands/cmd_wifi.cpp" "commands/cmd_traffic.cpp"
            "commands/command_status.cpp" "commands/cmd_tasks.c" "system/task_stats.c"
            "usb/usb_device.cpp" "usb/usb_protocol.cpp" "usb/usb_console.cpp"
            ${usb_service_sources}
            INCLUDE_DIRS "." "log" "commands" "system"
            PRIV_REQUIRES airgrep loop hwtest display console esp_timer esp_driver_gpio
            esp_tinyusb esp_pm esp_app_format esp_hw_support)
endif ()
# Wraps an esp_private symbol of the log component and the console's terminal probe.
# Verified on ESP-IDF v6.1, re-check on upgrades.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_log_level_get_timeout"
        "-Wl,--wrap=linenoiseProbe")
if(CONFIG_TINYUSB_CDC_ENABLED AND NOT test)
    # IDF 6.1's stdio REPL otherwise installs a built-in console driver.
    target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_stdio_install_io_driver")
endif()

idf_build_get_property(test AIRGREP_TEST)
if (test)
    idf_component_register(SRCS "test_main.c" "log/log_ring.c" "test/test_log_ring.c"
            "log/log_session.c" "test/test_log_session.c" "system/task_stats.c" "test/test_task_stats.c"
            INCLUDE_DIRS "." "log" "commands" "system" PRIV_REQUIRES unity airgrep WHOLE_ARCHIVE)
else ()
    idf_component_register(SRCS "main.cpp" "console.c" "banner.c" "commands/cmd_uptime.c"
            "commands/cmd_reboot.c" "commands/cmd_logs.c" "log/log_ring.c" "log/log_session.c" "commands/cmd_exp.cpp"
            "commands/cmd_charger.cpp" "commands/cmd_battery_level.cpp" "commands/cmd_baro.cpp"
            "commands/cmd_fusb.cpp" "commands/cmd_beep.cpp" "commands/cmd_adsbee.cpp" "commands/cmd_gnss.cpp" "commands/cmd_clock.cpp"
            "commands/command_status.cpp" "commands/cmd_tasks.c" "system/device_info.c" "system/task_stats.c"
            INCLUDE_DIRS "." "log" "commands" "system"
            PRIV_REQUIRES airgrep loop hwtest console esp_app_format esp_timer efuse spi_flash esp_psram)
endif ()
# Wraps an esp_private symbol of the log component. Verified on ESP-IDF v6.1, re-check on upgrades.
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_log_level_get_timeout")

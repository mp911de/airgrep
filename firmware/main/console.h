#pragma once

#include "esp_console.h"

#define CONSOLE_LABEL_WIDTH 16
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t console_start(void);

// The command, help and hint strings must remain valid for the life of the
// console.
esp_err_t console_register(const esp_console_cmd_t* command);
void console_banner(void);
esp_err_t console_register_uptime(void);
esp_err_t console_register_tasks(void);
esp_err_t console_register_reboot(void);
esp_err_t console_register_exp(void);
esp_err_t console_register_charger(void);
esp_err_t console_register_battery_level(void);
esp_err_t console_register_baro(void);
esp_err_t console_register_fusb(void);
esp_err_t console_register_beep(void);
esp_err_t console_register_adsbee(void);
esp_err_t console_register_gnss(void);
esp_err_t console_register_clock(void);
esp_err_t console_register_settings(void);
esp_err_t console_register_wifi(void);
esp_err_t console_register_traffic(void);
esp_err_t console_register_hwtest(void);
esp_err_t console_register_conflicts(void);

#ifdef __cplusplus
}
#endif

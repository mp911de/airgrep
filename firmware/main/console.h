#pragma once

#include "esp_console.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t console_start(void);
// Registers with esp_console and lists the command in help.
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

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shared 3.3 V bus, external pull-ups on the carrier.
// One transaction blocks for CONFIG_AIRGREP_I2C_TIMEOUT_MS at most, so an
// action on the loop does one transaction. Never address 0x7E, the I3C
// broadcast address, it would lock the BMP580 into I3C mode.
#define I2C_BUS_SPEED 400000

esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);
bool i2c_bus_present(uint8_t address);
// Adds the device when it acknowledges its address. ESP_ERR_INVALID_STATE without a bus,
// ESP_ERR_NOT_FOUND when nothing answers, otherwise the add result.
esp_err_t i2c_bus_add_if_present(const i2c_device_config_t* config, i2c_master_dev_handle_t* device);

#ifdef __cplusplus
}
#endif

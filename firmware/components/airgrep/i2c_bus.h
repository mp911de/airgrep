#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Shared 3.3 V bus, external pull-ups on the carrier.
#define I2C_BUS_SPEED 400000

esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);
// Adds the device when it acknowledges its address. A device that is already
// added stays. ESP_ERR_INVALID_STATE without a bus, ESP_ERR_NOT_FOUND when
// nothing answers, otherwise the add result.
esp_err_t i2c_bus_add_if_present(const i2c_device_config_t* config,
								 i2c_master_dev_handle_t* device);

#ifdef __cplusplus
}
#endif

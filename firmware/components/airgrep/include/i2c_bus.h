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

esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);
bool i2c_bus_present(uint8_t address);

#ifdef __cplusplus
}
#endif

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_err.h"

// Shared 3.3 V bus, 400 kHz, external pull-ups on the carrier.
// Scans cover 0x08 to 0x77 only. 0x7E is the I3C broadcast address and
// would lock the BMP580 into I3C mode.

#define I2C_BUS_HZ 400000
#define I2C_BUS_SCAN_FIRST 0x08
#define I2C_BUS_SCAN_LAST 0x77

esp_err_t i2c_bus_init(void);
i2c_master_bus_handle_t i2c_bus_handle(void);
bool i2c_bus_present(uint8_t address);
void i2c_bus_log_scan(void);

#pragma once

#include <cstddef>

#include "bmp580_i2c.hpp"
#include "device.h"
#include "esp_err.h"
#include "i2c_bus.h"
#include "locked_state.hpp"
#include "loop.hpp"

namespace barometer {
#define BMP_I2C_ADDR 0x46
#define BMP_STARTUP_MS 4
// tstandby is 2.5 ms, rounded up to whole loop milliseconds.
#define BMP_STANDBY_MS 3
#define BMP_POLL_PERIOD_MS 2000
// The switch to NORMAL starts the first measurement: tstartup_deep 4 ms, then
// tconv_p 5.4 ms at x8 and tconv_t 1.1 ms at x2, each +5 %.
#define BMP_FIRST_SAMPLE_MS 12
// BMP580 datasheet: register map section 7, soft reset 4.3.10, timings
// table 4.
constexpr uint8_t kRegChipId = 0x01;
constexpr uint8_t kRegIntConfig = 0x14;
constexpr uint8_t kRegFifoSel = 0x18;
constexpr uint8_t kRegTempDataXlsb = 0x1d;
constexpr uint8_t kRegStatus = 0x28;
constexpr uint8_t kRegDspIir = 0x31;
constexpr uint8_t kRegOsrConfig = 0x36;
constexpr uint8_t kRegOdrConfig = 0x37;
constexpr uint8_t kRegOsrEff = 0x38;
constexpr uint8_t kRegCmd = 0x7e;
constexpr uint8_t kChipIdBmp580 = 0x50;
constexpr uint8_t kCmdSoftReset = 0xb6;
constexpr uint8_t kIntOpenDrain = 1 << 2;
constexpr uint8_t kIntEnable = 1 << 3;
constexpr uint8_t kPowerModeStandby = 0;
constexpr uint8_t kPowerModeNormal = 1;
constexpr uint8_t kOdrRateHalfHz = 0x1d;
constexpr uint8_t kOdrConfigStandby =
	kOdrRateHalfHz << kOdrRateShift | kPowerModeStandby;
constexpr uint8_t kOdrConfigNormal =
	kOdrRateHalfHz << kOdrRateShift | kPowerModeNormal;
constexpr uint8_t kOsrConfig = 3 << kOsrPressureShift | 1;
constexpr uint8_t kOsrEffectiveValid = 1 << 7;

const i2c_device_config_t i2c_config = {
	.dev_addr_length = I2C_ADDR_BIT_LEN_7,
	.device_address = BMP_I2C_ADDR,
	.scl_speed_hz = I2C_BUS_SPEED,
	.scl_wait_us = 0,
	.flags = {},
};

struct State : StateData, LockedState<State> {};

using Completion = loop::Callable<void(esp_err_t)>;
// Subscribes to mode_changed: the sensor measures in ON only.
esp_err_t prepare();
// Starts the driver unless it polls already. The system watchdog calls it
// again on the loop for a driver that stopped or never showed up. Every
// measurement is published as barometer_reading.
void init(Completion complete);
// Posts the soft reset. The outcome shows in State on the next poll.
esp_err_t reset();
}

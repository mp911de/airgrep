# Display firmware (ESP32-S3)

Requirements the carrier puts on the display firmware. Controls (mute, Wi-Fi, expander polling) are in [workflows/physical-controls.md](../workflows/physical-controls.md); pin maps in [interfaces.md](interfaces.md).

## Operating modes

The display gets 5 V from two ideal diodes: U12 from the 5 V rail (POWER slider on) and U14 from the charger's PMID node (USB plugged in). The ADSBee only runs from the 5 V rail.

| USB present (BQ25798 VBUS_STAT) | U13 P7 SW_ON_N | Mode |
|---|---|---|
| yes | high (slider off) | charger mode: display only, ADSBee unpowered |
| yes | low (slider on) | normal operation, charging in the background |
| no | low | battery operation |
| no | high | everything off, the ESP is unpowered |

- SW_ON_N is the POWER slider through Q11: low means slider on. Low with the thermostat open is not possible (THERM_OK feeds the slider), so slider off plus no charging current can also mean "pack hot"; read the charger's TS status before showing a reason.
- In charger mode the ESP must not set EN_HIZ on the charger. PMID, and with it the ESP's own supply, drops in HIZ.
- In charger mode drive ESP_UART_TX (D13) low or set it as input. The ADSBee is unpowered and would be fed through R30 (1 kΩ, about 3 mA).

## ADSBee UART

- D12 RX, D13 TX. 115200 baud, 8E1, no RTS/CTS. ESP-IDF: `UART_DATA_8_BITS`, `UART_PARITY_EVEN`, `UART_STOP_BITS_1`, `UART_HW_FLOWCTRL_DISABLE`.
- Provision ADSBee through USB: `AT+BAUD_RATE=COMMS_UART,115200`, `AT+PROTOCOL_OUT=COMMS_UART,CSBEE`, `AT+SETTINGS=SAVE`. Existing saved baud rates survive firmware updates.
- Send AT commands with LF or CRLF. Keep one command outstanding. Separate CSBEE lines (`#` prefix) from AT replies and verify CSBEE CRC16.
- On parity, framing or buffer-overrun errors, discard through a clean LF. For a missing/corrupt reply, query state before retrying a state change. Parity cannot detect every corruption.
- The link carries at most 10472 bytes/s. Check dense-traffic load on hardware before accepting the reporting rate.
- Build the local ADSBee fork with `ADSBEE_PROFILE=airgrep bash firmware/build.sh adsbee_1090`. Default builds use 8N1 without UART commands. Deployment, display integration and physical UART verification remain open.

## Charger host control (BQ25798, I2C 0x6B)

- Set REG14 bit 7 (`SFET_PRESENT`) using read-modify-write during initialization and after register reset. Q13 is populated; the POR default is zero and disables ship-FET features. Read back the bit before treating the QON reset button as available. Firmware implementation remains required.
- SW4 held for about 10 s requests the charger's hardware system power reset, about 350 ms off. D10 holds the USB/PMID display feed off until release. Reinitialize peripherals after boot; the charger and MAX17048 stay powered and retain state.

- Without a host the charger runs its defaults: 1 A, TREG 120 °C, watchdog 40 s. The hardware TS window is 3 to 43 °C.
- The host may raise ICHG to 1.8 A and set TREG to 100 °C (or 80 °C). Every register marked "reset by WATCHDOG" falls back when the watchdog expires: reset the watchdog well inside 40 s, and re-apply the whole profile whenever WD_STAT is set.
- INT is not wired to the ESP (TP8 only). Poll STAT, faults and the ADC (VBUS, IBUS, IBAT, VBAT, VSYS, TS, TDIE) every 1 to 2 s while awake.
- Input current: the display load on PMID counts inside IINDPM. Keep IINDPM at the detected source limit (500 mA on a standard USB port).
- Adaptive current: lower ICHG when TDIE approaches TREG or the pack approaches the 43 °C stop; charger mode with the modules off allows the full 1.8 A in the closed case (heat model, device-off case).

## I2C bus

Bus at 3.3 V, 100 kHz, the module's 5.1 kΩ pull-ups. Datasheet cross-check of all four carrier slaves (TCA9534, MAX17048, BMP580, BQ25798): levels, speed and timing are compatible, nothing to change on the board. Rules the parts impose:

- 100 kHz only. Estimated bus capacitance is about 130 pF per line (the MAX17048 alone is 60 pF per pin), rise time about 560 ns with 5.1 kΩ. Fast mode needs R19/R20 fitted and is still marginal.
- Never address 0x7E. It is the I3C broadcast address and locks the BMP580 into I3C mode until power-off. Bus scans run 0x08 to 0x77 only.
- MAX17048: 16-bit words only, both bytes in one transaction. Keep MODE.EnSleep at 0 (default); with it set, the bus at 0 V while the display is off puts the gauge to sleep after 2.5 s. Check STATUS.RI at each power-up; set means the gauge lost power, reload the config.
- No ACK is normal for the MAX17048 POR command (0x5400 to 0xFE) and the BMP580 soft reset (0xB6 to 0x7E). Do not treat it as a bus error.
- BQ25798: a transaction not finished within 1.5 s is dropped and the chip returns to idle. WD_FLAG clears on read. Thresholds are absolute (VIH 1.3 V, VIL 0.4 V), fine at 3.3 V. I2C works on battery only, in HIZ and in ship mode, not in shutdown.
- BMP580: wait 2 ms after ESP_3V3 before the first access, then check CHIP_ID = 0x50 and STATUS.nvm_rdy. Keep INT_CONFIG.int_en disabled because INT is grounded.
- TCA9534: reads NACK until the first command byte has been written after power-up.

## Charger-mode display

- Show: charging state (pre-charge, fast, taper, done, fault), current setting (default 1 A or host 1.8 A), charge current, pack temperature, state of charge and remaining time.
- State of charge from the MAX17048 (0x36). Remaining time: capacity times (1 − SoC) over the measured charge current, plus a taper term for the constant-voltage phase, otherwise the last 20 % reads short.
- Backlight: dim when charger power appears or a touch is registered, off after 10 s.

## Sleep

- In charger mode the ESP may deep-sleep between updates. Wake on touch, and on a timer short enough to reset the charger watchdog (below 40 s, for example every 30 s); after a missed wake the charger falls back to the 1 A default, which is safe.
- On wake, re-check VBUS_STAT, WD_STAT and the faults before re-applying the profile.

## USB unplug and plug

- Plugging or unplugging USB must not reset the ESP or the ADSBee in normal operation. The ADSBee rail does not depend on USB. With the slider on, Q12 keeps U14 off, so the display always runs from U12 and an unplug does not touch its feed. C51/C52 cover the changeover when the slider is moved with USB present.
- In charger mode an unplug removes the ESP's supply. Nothing to save beyond what the display shows.

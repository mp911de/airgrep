# Display firmware (ESP32-S3)

Requirements the carrier puts on the display firmware. Pin maps and electrical connections are in [interfaces.md](interfaces.md), implementation progress in [firmware-plan.md](firmware-plan.md), bench checks in [open-issues.md](open-issues.md).

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
- In charger mode drive ESP_UART_TX (display D13, Plus GPIO44) low or set it as input. The ADSBee is unpowered and would be fed through R30 (1 kΩ, about 3 mA).

## ADSBee UART

- 115200 baud, 8E1, no RTS/CTS, on the pins in [interfaces.md](interfaces.md). On the Plus, UART1 is routed through the GPIO matrix. ESP-IDF: `UART_DATA_8_BITS`, `UART_PARITY_EVEN`, `UART_STOP_BITS_1`, `UART_HW_FLOWCTRL_DISABLE`.
- The Plus profile omits initialization of the original display, touch controller and IMU.
- Provision ADSBee through USB: `AT+BAUD_RATE=COMMS_UART,115200`, `AT+PROTOCOL_OUT=COMMS_UART,CSBEE`, `AT+SETTINGS=SAVE`. Existing saved baud rates survive firmware updates.
- Send AT commands with LF or CRLF. Keep one command outstanding. Separate CSBEE lines (`#` prefix) from AT replies and verify CSBEE CRC16.
- On parity, framing or buffer-overrun errors, discard through a clean LF. For a missing/corrupt reply, query state before retrying a state change. Parity cannot detect every corruption.
- The link carries at most 10472 bytes/s. Check dense-traffic load on hardware before accepting the reporting rate.
- Build the local ADSBee fork with `ADSBEE_PROFILE=airgrep bash firmware/build.sh adsbee_1090`. Default builds use 8N1 without UART commands.

## Physical controls

- Poll U13 at 0x20, debounce SW2/SW3 and apply both positions at startup. Keep PWM silent until the first valid mute read.
- The touchscreen displays slider states without overriding them. Low battery may dim the backlight but must not turn off Wi-Fi requested by SW3.
- Retain both last known states if a read fails. Show the shared peripheral-failure indication for expander, gauge or barometer faults.
- P0/P1/P7 remain inputs, with polarity inversion disabled. Read the input-port register at 0x00. P4-P6 on J10 are RST_1, RST_2 and LCD_LED for an optional external display. Leave them as inputs unless that display is fitted. Before enabling outputs, preload P4/P5 high (resets released) and P6 low (backlight off), preserving the P2/P3 output bits. The existing 100 kΩ pull-ups default these signals high before initialization. Backlight control through U13 is on/off only.

## GNSS control

- Optional JP4 connects GNSS_TXO to standalone XIAO Plus D10/GPIO9 for a second UART RX. Keep GPIO9 input-only with internal pulls disabled through startup and charger mode; never transmit on it. Match ADSBee's configured GNSS baud rate and parse its enabled message stream. JP4 must remain open with the original display fitted because its J1.4 pad is GND.
- U13 P2/P3 control RESET_N/SAFEBOOT_N. Initialize their output-port bits to zero; use the direction bits to pull low as outputs or release as inputs. Never drive them high. D11/D12 isolate the receiver's switched supply from the expander pull-ups.
- Reset: hold P2 low for at least 1 ms, then release.
- Safeboot recovery: hold P3 low while releasing P2, then release P3. Keep P3 an input during normal operation; it cannot read TIMEPULSE through D12. PPS uses its dedicated line.


## Charger host control (BQ25798, I2C 0x6B)

- Set REG14 bit 7 (`SFET_PRESENT`) using read-modify-write during initialization and after register reset. Q13 is populated; the POR default is zero and disables ship-FET features. Read back the bit before treating the QON reset button as available.
- SW4 held for about 10 s requests the charger's hardware system power reset, about 350 ms off. D6 holds the USB/PMID display feed off until release. Reinitialize peripherals after boot; the charger and MAX17048 stay powered and retain state.

- Without a host the charger runs its defaults: 1 A, TREG 120 °C, watchdog 40 s. The hardware TS window is 3 to 43 °C.
- The host may raise ICHG to 1.8 A and set TREG to 100 °C (or 80 °C). Every register marked "reset by WATCHDOG" falls back when the watchdog expires: reset the watchdog well inside 40 s, and re-apply the whole profile whenever WD_STAT is set.
- INT is not wired to the ESP (TP8 only). Poll STAT, faults, the ADC (VBUS, IBUS, IBAT, VBAT, VSYS, TS, TDIE) and the limits and controls REG03 to REG16 every 250 ms while awake, the watchdog write last. The status read stays before the watchdog write, because the write clears WD_STAT. Flag registers clear on read (WD_FLAG, data sheet 7.4.1), so the Poll reads no further flag registers.
- Input current: the display load on PMID counts inside IINDPM. Keep IINDPM at the detected source limit (500 mA on a standard USB port).
- USB-C initialization: wait 100 ms after U15 (FUSB303B, 0x21) has VDD valid and EN_N low before its first I2C access (`tEN`, datasheet page 9), then set `CONTROL1.ENABLE` (register 0x05 bit 3). Poll `STATUS.ATTACH`, `STATUS.BC_LVL` and `STATUS.VBUSOK`; require attached, VBUS valid and `TYPE.SINK` before using the advertised current.
- USB source current follows Charge control below. Default CC on an A-to-C cable does not cap a detected charger at 500 mA. A replug reruns BC1.2 and overwrites IINDPM, so the policy is reapplied after detection completes. EN_EXTILIM survives the watchdog, only REG_RST resets it. Current precedence follows [USB-IF section 4.6](https://usb.org/sites/default/files/USB%20Type%20C%20Functional%20Test%20Specification%202019%2004%2030.pdf); D+/D- results are in BQ25798 table 7-4.

### Charge control

Charge control in the system component sets IINDPM, ICHG, EN_CHG, TREG and EN_EXTILIM from the USB source, the pack capacity and the pack temperature. It runs after every complete charger Poll and USB-C controller Poll (both 250 ms) and on a 2 s safety run. The operating mode is not an input, ON and CHARGER mode follow the same rules.

- A driver State is usable while its last successful Poll is at most 5 s old. One failed Poll does not make it unusable. The ADSBee TX gate keeps its own 500 ms limit.
- Wait: while the USB-C controller negotiates (VBUS but not attached, or REMEDY) or the charger detects (VBUS and BC1.2_DONE_STAT clear), EN_EXTILIM is set and nothing else on the input side is written. The wait ends on completion or 5 s after VBUS appeared.
- Input target: a 1.5 A or 3 A CC advertisement wins. Otherwise the charger's detection result: SDP 500 mA, CDP, DCP and HVDCP 1500 mA, non-standard adapter 1000 mA (the register does not say which divider), anything else 500 mA. A missing or stale USB-C State counts as default CC. No reserve is subtracted, the charger feeds the system first.
- EN_EXTILIM is cleared only for a valid 3 A advertisement. On detach or VBUS loss it is set at once. It is always set before IINDPM is lowered and cleared only after IINDPM was raised.
- Charge target: 0.5 C of the configured pack capacity, 500 to 2000 mA. At or above 33 °C pack temperature at most 1000 mA, released at 31 °C. At or above 36 °C at most 500 mA, released at 34 °C.
- Charge enable: off at or below 4 °C, released at 6 °C. Off at or above 40 °C, released at 38 °C.
- A lost pack temperature keeps the last decision of every threshold. Without any valid reading since start, charging runs at 500 mA.
- Order for charge enable: temperature, then battery overvoltage, then the rest.
- Battery overvoltage: VBAT_OVP (live or latched) switches charging off. It counts as one failure only when charging was on. After 5 min off with VBAT_OVP gone, VBAT decides: below 2.6 V is no pack, because without a pack and with charging off the charger holds BAT close to 2.5 V (data sheet 7.3.5). A pack is found later when VBAT rises to 2.6 V. The failure count resets once the last failure is more than 10 min old. At 10 failures charging is locked out until a restart, RAM only. A VBAT drop does not clear the lockout, an opened pack protection reads like a removed pack.
- TREG is 100 °C. No firmware rule acts on TDIE, the chip regulates it.
- Reconciliation: the target is compared with the registers the last Poll read back, only differing registers are written. A failed write is retried on the next run.
- Control mode AUTO or MANUAL lives in RAM, AUTO after every start. In MANUAL charge control writes nothing and `charger --iindpm`, `--ichg`, `--enable-charge`, `--disable-charge` and the battery tests are allowed. `--iindpm` above the hardware input limit is rejected while EN_EXTILIM is set. No Command clears EN_EXTILIM. `--mode` is rejected while a battery test runs.
- The event loop task is guarded by the 5 s task watchdog, which restarts the device.

## I2C bus

Use a 3.3 V bus at up to 400 kHz. Pull-up configuration, electrical limits and bench acceptance are in [I2C bus requirements](i2c-bus-requirements.md). Rules the parts impose:

- Never address 0x7E. It is the I3C broadcast address and locks the BMP580 into I3C mode until power-off. Bus scans run 0x08 to 0x77 only.
- MAX17048: 16-bit words only, both bytes in one transaction. Keep MODE.EnSleep at 0 (default); with it set, the bus at 0 V while the display is off puts the gauge to sleep after 2.5 s. Check STATUS.RI at each power-up; set means the gauge lost power, reload the config.
- No ACK is normal for the MAX17048 POR command (0x5400 to 0xFE) and the BMP580 soft reset (0xB6 to 0x7E). Do not treat it as a bus error.
- BQ25798: a transaction not finished within 1.5 s is dropped and the chip returns to idle. WD_FLAG clears on read. Thresholds are absolute (VIH 1.3 V, VIL 0.4 V), fine at 3.3 V. I2C works on battery only, in HIZ and in ship mode, not in shutdown.
- BMP580: wait 2 ms after ESP_3V3 before the first access, then check CHIP_ID = 0x50 and STATUS.nvm_rdy. INT is intentionally unconnected. Follow Bosch section 6.2: set INT_CONFIG.int_en=1 and int_od=0 (push-pull); poll measurement/status registers. The local datasheet leaves the recommended pad_int_drv value blank, so retain its reset value.
- FUSB303B: address 0x21 from the ADDR low strap. Powered from ESP_3V3, so it is never unpowered on a live bus.
- TCA9534: reads NACK until the first command byte has been written after power-up.

## Charger-mode display

- Show: charging state (pre-charge, fast, taper, done, fault), current setting (default 1 A or host 1.8 A), charge current, pack temperature, state of charge and remaining time.
- State of charge from the MAX17048 (0x36). Remaining time: capacity times (1 − SoC) over the measured charge current, plus a taper term for the constant-voltage phase, otherwise the last 20 % reads short.
- Backlight: dim when charger power appears or a touch is registered, off after 10 s.

## Sleep

- In charger mode the ESP may deep-sleep between updates. Wake on touch, and on a timer short enough to reset the charger watchdog, which the firmware sets to 20 s (for example every 15 s); after a missed wake the charger falls back to the 1 A default, which is safe.
- On wake, re-check VBUS_STAT, WD_STAT and the faults before re-applying the profile.

## USB unplug and plug

- Plugging or unplugging USB must not reset the ESP or the ADSBee in normal operation. The ADSBee rail does not depend on USB. With the slider on, Q12 keeps U14 off, so the display always runs from U12 and an unplug does not touch its feed. C51/C52 cover the changeover when the slider is moved with USB present.
- In charger mode an unplug removes the ESP's supply. Nothing to save beyond what the display shows.

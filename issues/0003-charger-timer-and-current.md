# Charger safety timer and charge current on the 10 Ah pack

Status: resolved by selecting BQ25798. MAX77757 development is closed.
Created: 2026-10-09

The frozen MAX77757 board's six-hour timer and fixed thermal regulation motivated the charger change. The active BQ25798 has a twelve-hour default timer and host-programmable current, thermal regulation and timer settings. Default charge current is 1 A; host operation targets 1.8 A.

Current host requirements are in [firmware.md](../docs/firmware.md#charger-host-control-bq25798-i2c-0x6b). Actual full-charge time, weak-source behavior and enclosure temperatures remain bench checks in [open-issues.md](../docs/open-issues.md).

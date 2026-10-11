# Charger safety timer and charge current on the 10 Ah pack

Status: resolved by the carrier-ti-charger variant; open for carrier-max-charger
Created: 2026-10-09

## Motivation

The MAX77757 fast-charge timer is 6 h ± 20 %, so 4.8 h minimum. A full charge of the 10 Ah pack at 1.8 A takes about 5 h 55 min, more from a weak source, so the pack can stop part-full with no automatic retry (FMEA C5). Raising IFAST shortens the charge but adds heat in a part that already regulates at 130 °C in a closed case.

## Resolution

- carrier-ti-charger: the BQ25798 fast-charge timer is 12 h by default and host-programmable (5 to 24 h, or off); the host sets 1.8 A and the timer, and the default mode charges at 1 A. Pack temperature and die temperature are readable over I2C.
- carrier-max-charger: no board change. Options remain as listed in `docs/power.md` (Earlier charge limiting).

## References

- `docs/fmea.md` C5
- `docs/charger-comparison.md`

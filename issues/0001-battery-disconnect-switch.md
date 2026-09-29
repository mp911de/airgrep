# Battery disconnect switch for the off state

Status: planned, next revision
Created: 2026-10-04

## Motivation

With the slider off the board still draws about 51 µA from the pack, most of it the charger's quiescent current, and a switch in the battery lead removes all of it at once.

## Options considered

| Option | Saves | Notes |
|---|---|---|
| R10 from 1 MΩ to 3.3 MΩ | 3 µA | value change only |
| High-side switch for the boost feedback divider R14/R15 | 11 µA | two SOT-23 parts; a low-side switch is not possible, FB is rated 3.6 V |
| Higher divider impedance | 5 µA | not recommended, noisy FB node next to the receiver |
| Charger with ship mode | 30 µA or more | redesign of the charger sheet |
| P-channel MOSFET in the battery lead | all of it | this issue; replaces the first two options |

## Plan

Coordinate this proposal with [0002: reverse-polarity protection](0002-battery-reverse-polarity-protection.md), which selects CSD25404Q3 MOSFETs for that function. The circuit below only provides off-state disconnect; its direct slider and USB gate control does not implement reverse-polarity protection. If the functions share a stage, redesign the control so reverse-polarity isolation takes precedence and recheck depleted-pack recovery and inrush control.

```
pack+ ── F1 ──┬── S [Q10] D ──── VBAT (charger BATT, gauge, rest of the board)
              │      G
              ├─ 1M ─┤
              └ 100n ┤
                     ├── slider, ON position ── GND
                     └── 2N7002, gate from USB_VBUS (as Q9/R42) ── GND
```

- Source towards the pack. The body diode then blocks discharge and conducts in the charging direction, so a deeply discharged pack still gets its precharge.
- Off by default through the 1 MΩ resistor. On when the slider is on or USB is present.
- The 100 nF capacitor slows the turn-on to limit the inrush into the VSYS capacitors.
- The slider needs a second pole so that the enable chain still knows its position while USB keeps the board powered.
- The undervoltage latch must not drive Q10. It would lose its own supply and cycle.
- The fuel gauge stays on the switched side and restarts from a voltage estimate at power-on.
- Bring-up check: measure the drop across Q10 while charging. Conduction through the body diode would be about 1.3 W.

## Parts

| Ref | Part | Notes |
|---|---|---|
| Q10 | Vishay Si7137DP, P-channel 20 V, PowerPAK SO-8 | 3.9 mΩ max at 2.5 V gate drive, LCSC C784367 |
| Q11 | 2N7002 | already in the BOM |
| R | 1 MΩ, 0603 | gate to source |
| R | 1 MΩ, 0603 | USB_VBUS to Q11 gate |
| C | 100 nF, 0603 | gate to source |
| SW1 | two-pole slider of the same C&K JS family | replaces the single-pole part; check the footprint |

## Numbers

| | Before | After |
|---|---|---|
| Off-state drain | 51 µA | about 1 µA |
| Per year | 0.45 Ah | 0.01 Ah |
| Loss in the switch at 1.74 A battery current | none | 14 mW |
| Voltage drop at 1.74 A | none | 8 mV |
| Battery path resistance on the board | about 70 mΩ | about 75 mΩ |

For scale: the 10 Ah pack loses the equivalent of 300 to 400 µA through self-discharge.
The 1 µA is a typical figure. The data sheet guarantees 1 µA leakage at 25 °C and 20 V only.
The undervoltage-latched state stays at about 60 µA until the slider is switched off.

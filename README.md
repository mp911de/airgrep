<h1>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="assets/logo-dark@2x.png">
    <img src="assets/logo@2x.png" alt="airgrep" width="305">
  </picture>
</h1>

A portable ADS-B / FLARM receiver based on [ADSbee](https://github.com/PantsForBirds/adsbee). Started as carrier board for a removable ESP32-S3 touch display and [ADSBee 1090U](https://pantsforbirds.com/product/adsbee-1090u/) receiver, with a soldered MAX-M10S GNSS receiver. It adds a single-cell Li-ion charger, a 5 V boost rail, a fuel gauge, a barometer and a buzzer.

<img src="hardware/carrier-ti-charger/render/PCBA%20Render%20F.png" alt="PCBA Render" style="width:400px;"/>

## Current state

- Schematic (7 sheets) and board (105 x 95 mm, 4 layers, all SMD parts on top) are implemented. The active TI PCB and schematics define the hardware; `tools/check.sh` reports the ERC, DRC and parity state.
- Order status and open work are in [docs/open-issues.md](docs/open-issues.md).

## Where things are

| Path | Content |
|---|---|
| [docs/specs.md](docs/specs.md) | Technical data: radio, charging, battery, power budget, runtimes and validation limits |
| [docs/decisions.md](docs/decisions.md) | Reasons behind the design decisions |
| [docs/mechanical.md](docs/mechanical.md) | Board outline, mounting holes, module placement, battery, antennas, enclosure |
| [docs/interfaces.md](docs/interfaces.md) | Pin maps of the modules and the carrier connectors |
| [docs/i2c-bus-requirements.md](docs/i2c-bus-requirements.md) | I2C speed, bandwidth, capacitance and pull-up limits |
| [docs/firmware.md](docs/firmware.md) | What the display firmware must do: operating modes, charger host control, charger-mode display, sleep |
| [docs/usb-data-protocol.md](docs/usb-data-protocol.md) | USB identity, binary data protocol and debug service commands |
| [docs/bom-hand-sourced.md](docs/bom-hand-sourced.md) | Parts to buy and fit by hand, with order links and order status |
| [docs/open-issues.md](docs/open-issues.md) | Open work only |
| [issues/](issues/) | Local issue tracker, one markdown file per issue |
| `specs/power_params.json` | Power model inputs of the power, runtime and charging tables in `docs/specs.md` |
| `hardware/carrier-ti-charger/` | Active KiCad 10 project `carrier.kicad_pro`, BQ25798 charger. `carrier-schematic.pdf` is the schematic print, `hardware/carrier-ti-charger/jlcpcb/` holds the production exports. |
| `hardware/carrier-max-charger/` | Frozen MAX77757 reference, no further development. Legacy tools and results remain available with `--project NAME`. |
| `hardware/insulator/` | KiCad project of the insulator board between carrier and battery |
| `hardware/sim/` | Verification by simulation, see [hardware/sim/README.md](hardware/sim/README.md). |
| `mechanical/` | Placement drawing (`placement.png`, `placement.svg`, generator `draw_placement.py`) and extracted module geometry (JSON) |
| `datasheets/` | Data sheets of the parts in use, see [datasheets/README.md](datasheets/README.md) |

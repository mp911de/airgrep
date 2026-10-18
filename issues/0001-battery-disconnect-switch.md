# Battery disconnect for the off state

Status: proposed, next revision. No circuit or replacement switch is selected.
Created: 2026-10-04

The POWER slider disables the receiver rail but leaves VBAT connected to the charger, gauge and GNSS backup supply. The existing partial standby budget is about 36 µA typical; measure total drain before deciding whether isolation is worthwhile.

The active board already has BQ25798 ship FET Q13. It does not isolate every raw-VBAT branch. The former MAX-board proposal, its Q10/Q11 references and C&K two-pole switch selection do not apply to this board.

Coordinate with [reverse-polarity protection](0002-battery-reverse-polarity-protection.md). If one stage serves both functions, reversed-pack isolation must take precedence over POWER and USB turn-on requests. Design gate drive, inrush and fault turn-off together.

Required checks:

- Preserve charging, USB-only operation and depleted-pack recovery.
- Define whether gauge and GNSS backup remain powered, and account for their drain.
- Keep the undervoltage latch stable when its supply is removed.
- Check losses, hot-plug stress and gate-drive limits over the cell range.
- Select a compatible control arrangement; the fitted HRO POWER slider is single-pole.

This remains a design proposal. No new off-state current or protection claim is established.

# Hardware review checklist

Review the current schematic, saved PCB copper and intended operating conditions. Each finding needs an affected circuit or path, evidence and an expected consequence. Distinguish recommendation deviations, model estimates and measured failures.

- [ ] **Ground returns:** Can switching or digital currents disturb sensitive references through shared impedance, narrow copper or return-path detours?
  Trace complete current loops through copper fills, vias and reference planes, then compare with the IC layout guidance, accounting for nearby shared stitching rather than requiring a via at every component.

- [ ] **Power integrity:** Do local bypasses and supply impedance support actual current pulses and load steps without excessive droop or noise?
  Cross-reference IC bypass requirements and load timing with routed supply/return loops, effective capacitance, upstream source impedance and operating-mode transitions.

- [ ] **Filters:** With capacitor voltage bias and bead current bias included, does the network attenuate noise without harmful resonance?
  Consult manufacturer impedance, resistance/reactance and bias curves plus capacitor ESR/ESL data, then evaluate transfer and supply impedance with realistic source/load impedances, sweeping assumptions where data is missing.

- [ ] **Component nonidealities:** Do noise, leakage, tolerance and drift matter here, and is there a circuit-specific reason for thin-film resistors?
  Identify the ordered parts from schematic properties, check their specifications and calculate their contribution against the circuit's noise, accuracy and threshold margins, including the IC's own errors.

- [ ] **Thermal margins:** Are steady dissipation, transient stress and temperature rise compatible with ratings and intended lifetime?
  Cross-reference worst-case voltage, current and duty cycle with manufacturer derating and pulse limits, using the actual copper, enclosure and thermal path when interpreting thermal models.

- [ ] **Implementation and verification:** Are schematics readable, pin mappings and footprints correct, relevant transmission-line geometry preserved, and critical measurements accessible?
  Compare signal flow and PCB pads with manufacturer pin/package drawings, check stackup and return continuity against interface edge rates and impedance requirements, and identify practical probe points for unresolved questions.

Based on Hans Rosenberg's [grounding cases](https://www.youtube.com/watch?v=pgJfOklQwNA), [PCB design skills](https://www.youtube.com/watch?v=esoZbu2WhS0) and [ferrite bead discussion](https://www.youtube.com/watch?v=S6Uxx5PCRQY). Their examples motivate these checks, not universal numerical acceptance limits.

# Firmware plan

Software progress across the ADSBee, rtl_433 and DAIDALUS workspaces. Checked items record implementation progress, not live RF or carrier acceptance; [open-issues.md](open-issues.md) owns hardware validation.

- [x] Implement FLARM FAMP in rtl_433 as proof of concept
- [ ] ADSBee
- [x] DAIDALUS to ESP port: native and S3 builds, optional file I/O; hardware tests deferred.
- [ ] DAIDALUS Python bindings. None found in the fork or in `components/daidalus`.

## ADSBee

- [x] Fork-friendly development to minimize conflicts
- [x] Enable bi-directional AT on UART (115200, 8E1, no flow control)
- [x] Repurpose the 978 MHz radio for FLARM
- [x] Add FLARM support (and define messages for ESP comms)
- [x] Add GDL90 support
- [x] show FLARM on Map
- [x] Integrate Wi-Fi slider control over the custom UART AT interface
- [ ] Disable any bluetooth. `adsbee_1090/esp/sdkconfig` still has `CONFIG_BT_ENABLED=y` and NimBLE, used when Remote ID RX is on.
- [ ] GNSS UART power sequencing for generic modules. They still drive TX while the receiver is off. The MAX-M10 path parks TX (`ParkTx()` in `gnss_receiver.cc`).

## ESP32

- [x] Setup project
- [x] Charge mode (adaptive temps, UI behavior, faults)
- [x] On mode
- [ ] BMP configure for charger mode, 1hz reading, oversampling, deep sleep. Standby outside ON and oversampling are done. The poll runs every 2 s, not 1 Hz.
- [ ] Define gui and functionality (buzzer, wifi, how to compute wifi and password from e.g. a serial number, show wifi credentials/QR code). The SSID comes from the device ID and the QR code is shown. The password is the fixed `airgrep-wifi`.
- [x] Obtain GPS position from ADSbee
- [x] BMP integration, calculate pressure altitude for ADS-B relations
- [x] Integrate DAIDALUS
- [x] How to avoid/ignore ownship so that a plane with flarm/transponder does not cause interference?
- [ ] Charger mode per [firmware.md](firmware.md): deep sleep between updates, and the display content (charging phase, current setting, charge current, pack temperature, taper term in the remaining time). Host control and charge control are done.
- [ ] Charger-mode backlight: dim on charger power or touch, off after 10 s.
- [ ] Low state of charge: optional backlight dimming. The warning is done as a health cause.
- [ ] Debounce SW2/SW3.
- [ ] GNSS safeboot recovery on U13 P2/P3. Reset exists only as the manual `exp --trigger` pulse.
- [ ] Optional JP4 second UART RX on GPIO9.


## Debugging/Develoment

- [ ] Traffic generator. USB traffic and GNSS injection exists, nothing sends it yet.
- [x] Python wrapper runtime
- [x] Visualization of traffic (3d space/geometry viewer, DAIDALUS conflict visualization)

# Firmware plan

Software progress across the ADSBee, rtl_433 and DAIDALUS workspaces. Checked items record implementation progress, not live RF or carrier acceptance; [open-issues.md](open-issues.md) owns hardware validation.

- [x] Implement FLARM FAMP in rtl_433 as proof of concept
- [ ] ADSBee
- [x] DAIDALUS to ESP port: native and S3 builds, optional file I/O and Python bindings; hardware tests deferred.

## ADSBee

- [x] Fork-friendly development to minimize conflicts
- [x] Enable bi-directional AT on UART (115200, 8E1, no flow control)
- [x] Repurpose the 978 MHz radio for FLARM
- [x] Add FLARM support (and define messages for ESP comms)
- [x] Add GDL90 support
- [x] show FLARM on Map
- [ ] Integrate Wi-Fi slider control over the custom UART AT interface
- [ ] Disable any bluetooth

## ESP32

- [ ] Setup project
- [ ] Charge mode (adaptive temps, UI behavior, faults)
- [ ] On mode
- [ ] Define gui and functionality (buzzer, wifi, how to compute wifi and password from e.g. a serial number, show wifi credentials/QR code)
- [ ] Obtain GPS position from ADSbee
- [ ] BMP integration, calculate pressure altitude for ADS-B relations
- [ ] Integrate DAIDALUS
- [ ] How to avoid/ignore ownship so that a plane with flarm/transponder does not cause interference?


## Debugging/Develoment

- [ ] Traffic generator
- [x] Python wrapper runtime
- [ ] Visualization of traffic (3d space/geometry viewer, DAIDALUS conflict visualization)

import Foundation

func testReadout() {
    for (percent, usage) in [(0, Usage.low), (50, .low), (51, .raised), (80, .raised), (81, .high), (100, .high)] {
        check(Usage(percent: percent) == usage, "\(percent) % is \(usage)")
    }
    check(Usage.percent(free: 148 * 1024, of: 390 * 1024) == 62, "used memory is what is not free")
    check(Usage.percent(free: 0, of: 0) == 0, "no memory is not used")
    check(Usage.percent(free: 9, of: 8) == 0, "more free than there is counts as all free")

    let minute = Date(timeIntervalSince1970: 1_795_356_300), utc = TimeZone(identifier: "UTC")!
    check(Export.name(of: "airgrep-a1b2c3d4e5f6", "console", at: minute, in: utc) == "airgrep-a1b2c3d4e5f6-console-2026-11-22_14_05.txt",
          "an export is named after device, content and minute: \(Export.name(of: "airgrep-a1b2c3d4e5f6", "console", at: minute, in: utc))")
    check(Export.name(of: "Demo Device", "data", at: minute, in: utc) == "demo-device-data-2026-11-22_14_05.txt",
          "the name of the demo device has no space")
}

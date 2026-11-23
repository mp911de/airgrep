import CryptoKit
import Foundation

/// One ESP application segment, the IDF descriptor followed by the airgrep board descriptor.
private func firmware(board: String, digest: Bool) -> [UInt8] {
    var segment = [UInt8](repeating: 0, count: 256)
    segment.replaceSubrange(0..<4, with: [0x32, 0x54, 0xCD, 0xAB])
    segment.replaceSubrange(16..<21, with: "1.2.3".utf8)
    segment.replaceSubrange(48..<55, with: "airgrep".utf8)
    segment += Array("airgrep-board:\(board)".utf8) + [0]
    var bytes = [UInt8](repeating: 0, count: 32)
    bytes[0] = 0xE9
    bytes[1] = 1
    bytes[12] = 9
    bytes[23] = digest ? 1 : 0
    bytes[28] = UInt8(truncatingIfNeeded: segment.count)
    bytes[29] = UInt8(segment.count >> 8)
    bytes += segment
    bytes += [UInt8](repeating: 0, count: 15 - bytes.count % 16)
    bytes.append(segment.reduce(0xEF, ^))
    if digest { bytes += SHA256.hash(data: bytes) }
    return bytes
}

func testImages() {
    let folder = FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
    let file = folder.appendingPathComponent("image.bin"), table = folder.appendingPathComponent("partitions.csv")
    let board = "S3 Plus"
    func read(_ bytes: [UInt8], _ kind: Maintenance.Kind = .firmware) throws -> Maintenance.Image {
        try Data(bytes).write(to: file)
        return try Maintenance.Image(file, kind, board: board, partitionsFile: table)
    }
    do {
        try FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        defer { try? FileManager.default.removeItem(at: folder) }
        try "factory,app,factory,0x10000,0x2000\ntzdata,data,0x40,0x20000,0x1000".write(to: table, atomically: true, encoding: .utf8)
        for digest in [true, false] {
            let bytes = firmware(board: board, digest: digest)
            let image = try read(bytes)
            check(image.summary == "Version: 1.2.3\nSize: 0 KB", "firmware summary shows version and size")
            try Data().write(to: file)
            check(image.bytes == bytes && image.partition == 0x10000..<0x12000, "install retains the checked firmware and its partition")
            var bad = bytes
            bad[12] = 0
            check(failure { _ = try read(bad) } != nil, "a different chip is refused")
            bad = bytes
            bad[bad.count - (digest ? 33 : 1)] ^= 1
            check(failure { _ = try read(bad) } != nil, "an incorrect image checksum is refused")
            if digest {
                bad = bytes
                bad[bad.count - 1] ^= 1
                check(failure { _ = try read(bad) } != nil, "an incorrect appended digest is refused")
            }
            check(failure { _ = try read(Array(bytes.dropLast())) } != nil, "an incomplete image is refused")
        }
        check(failure { _ = try read(firmware(board: "Seeed Display 1.47", digest: true)) } != nil, "another board profile is refused")
        // The installer reads the TZL2 header. One payload byte lets truncation leave the header intact.
        var tz = [UInt8](repeating: 0, count: 129)
        tz.replaceSubrange(0..<4, with: "TZL2".utf8)
        tz[40] = UInt8(tz.count)
        let source = "2026f/test".utf8
        tz.replaceSubrange(64..<(64 + source.count), with: source)
        let image = try read(tz, .timeZones)
        check(image.summary == "Version: 2026f\nSize: 0 KB", "time zone summary shows the release and size")
        check(image.bytes == tz, "a matching TZL2 length is accepted")
        check(failure { _ = try read(Array(tz.dropLast()), .timeZones) } != nil, "a truncated time zone table is refused")
        check(failure { _ = try read(Array(repeating: 0, count: 4097), .timeZones) }?.contains("partition") == true, "oversize files are refused")

        func dropped(_ bytes: [UInt8]) throws -> Maintenance.Image {
            try Data(bytes).write(to: file)
            return try Maintenance.Image(file, board: board, partitionsFile: table)
        }
        check(try dropped(firmware(board: board, digest: true)).kind == .firmware, "firmware is known by its magic")
        check(try dropped(tz).kind == .timeZones, "a time zone table is known by its magic")
        for other in [[], [0xE9], Array("PK\u{3}\u{4} not ours".utf8), [UInt8](repeating: 0xE9, count: 64)] {
            check(failure { _ = try dropped(other) }?.contains("neither") == true, "a file of \(other.count) other bytes is refused")
        }
        check(Maintenance.Image.kind(of: folder.appendingPathComponent("missing.bin")) == nil, "a file that is not there is nothing")
    } catch {
        check(false, error.localizedDescription)
    }
}

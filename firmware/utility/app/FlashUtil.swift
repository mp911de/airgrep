import CryptoKit
import Foundation
import zlib

struct Failure: LocalizedError {
    let errorDescription: String?
    init(_ description: String) { errorDescription = description }
}

/// The ROM bootloader of an ESP32-S3 as a flash writer: Espressif's serial protocol, spoken to the
/// ROM itself. No stub is loaded into the chip, so the application carries no code of Espressif.
final class FlashUtil {
    enum Step: Equatable {
        case erasing
        /// The share of the data that is on the device.
        case writing(Double)
        case verifying
    }

    private enum Command: UInt8 {
        case writeRegister = 0x09, readRegister = 0x0A, setFlashParameters = 0x0B, attachFlash = 0x0D
        case sync = 0x08, beginCompressed = 0x10, compressedData = 0x11, flashMD5 = 0x13
    }

    private enum Register {
        /// A variable of the ROM that names the port it talks on, and its value for USB Serial/JTAG.
        static let port: UInt32 = 0x3FCE_F14C, portSerialJTAG: UInt32 = 4
        static let watchdogProtect: UInt32 = 0x6000_80B0, watchdogKey: UInt32 = 0x50D8_3AA1
        static let watchdogConfiguration: UInt32 = 0x6000_8098
        static let superWatchdogProtect: UInt32 = 0x6000_80B8, superWatchdogKey: UInt32 = 0x8F1D_312A
        static let superWatchdogConfiguration: UInt32 = 0x6000_80B4, superWatchdogAutoFeed: UInt32 = 1 << 31
        /// Bit 0 keeps the chip in its bootloader across a reset.
        static let bootOption: UInt32 = 0x6000_812C, forceDownload: UInt32 = 1
    }

    /// What the ROM takes in one request.
    static let blockSize = 0x400
    private static let reply = Duration.seconds(3)
    private static let syncReply = Duration.milliseconds(100), syncAttempts = 30
    /// The ROM erases before it answers a begin, and writes a block before it answers that.
    private static let erasePerMegabyte = 30.0, writePerMegabyte = 40.0, digestPerMegabyte = 8.0
    private static let checksumSeed: UInt8 = 0xEF

    private let link: SerialLink
    /// What is left of a time that starts now, on the clock that measures every wait for the device.
    private let timeLeft: (Duration) -> () -> Duration
    private let pause: (Duration) async throws -> Void
    /// Bytes that arrived and belong to a frame not complete yet.
    private var received: [UInt8] = []

    init(_ link: SerialLink, clock: some Clock<Duration>) {
        self.link = link
        timeLeft = { timeout in
            let deadline = clock.now.advanced(by: timeout)
            return { clock.now.duration(to: deadline) }
        }
        pause = { try await clock.sleep(for: $0) }
    }

    /// Finds the ROM on the port and keeps it from resetting the chip while it is written.
    func connect() throws {
        var answered = false
        for _ in 0..<Self.syncAttempts where !answered {
            try Task.checkCancellation()
            link.discardInput()
            received = []
            answered = (try? exchange(.sync, [0x07, 0x07, 0x12, 0x20] + [UInt8](repeating: 0x55, count: 32), within: Self.syncReply)) != nil
        }
        try Task.checkCancellation()
        guard answered else {
            throw Failure("The bootloader does not answer.")
        }
        // The ROM answers a sync several times over.
        while (try? frame(within: timeLeft(Self.syncReply))) != nil {}
        try Task.checkCancellation()

        // Over USB Serial/JTAG nothing feeds the watchdogs of the chip.
        if try readRegister(Register.port) & 0xFF == Register.portSerialJTAG {
            try writeRegister(Register.watchdogProtect, Register.watchdogKey)
            try writeRegister(Register.watchdogConfiguration, 0)
            try writeRegister(Register.watchdogProtect, 0)
            try writeRegister(Register.superWatchdogProtect, Register.superWatchdogKey)
            try writeRegister(
                Register.superWatchdogConfiguration,
                try readRegister(Register.superWatchdogConfiguration) | Register.superWatchdogAutoFeed)
        }
    }

    /// Cancellation is safe until the erase starts. After that, finish writing and verify the digest.
    func write(_ image: [UInt8], in partition: Range<Int>, flashSize: Int, progress: (Step) -> Void) throws {
        let offset = partition.lowerBound
        // The ROM erases whole 4 KB sectors and accepts 32 bit sizes.
        guard offset >= 0, offset.isMultiple(of: 4096), partition.upperBound.isMultiple(of: 4096),
              partition.upperBound <= flashSize, UInt32(exactly: flashSize) != nil,
              !image.isEmpty, image.count <= partition.count else {
            throw Failure("The image does not fit its flash partition.")
        }
        // The ROM writes whole words.
        let image = image + [UInt8](repeating: 0xFF, count: (4 - image.count % 4) % 4)
        let compressed = try Self.compress(image)
        let blocks = stride(from: 0, to: compressed.count, by: Self.blockSize).map {
            Array(compressed[$0..<min($0 + Self.blockSize, compressed.count)])
        }
        let erased = (image.count + Self.blockSize - 1) / Self.blockSize * Self.blockSize

        try check(.attachFlash, Self.words(0, 0), "Attaching the flash")
        try check(.setFlashParameters, Self.words(0, UInt32(flashSize), 64 * 1024, 4 * 1024, 256, 0xFFFF), "Setting the flash size")
        try Task.checkCancellation()
        progress(.erasing)
        try check(
            .beginCompressed, Self.words(UInt32(erased), UInt32(blocks.count), UInt32(Self.blockSize), UInt32(offset), 0),
            "Erasing", within: Self.time(Self.erasePerMegabyte, for: erased))

        // A block of zeros unpacks to far more than its size, so every block gets the time of the whole image.
        let blockTime = Self.time(Self.writePerMegabyte, for: image.count)
        for (sequence, block) in blocks.enumerated() {
            progress(.writing(Double(sequence) / Double(blocks.count)))
            try check(
                .compressedData, Self.words(UInt32(block.count), UInt32(sequence), 0, 0) + block,
                "Writing block \(sequence + 1) of \(blocks.count)",
                checksum: UInt32(block.reduce(Self.checksumSeed, ^)), within: blockTime)
        }
        // The ROM is not told that the data ended: it would leave the bootloader before the digest is read.

        progress(.verifying)
        let digest = try check(
            .flashMD5, Self.words(UInt32(offset), UInt32(image.count), 0, 0), "Reading the digest",
            replyBytes: 32, within: Self.time(Self.digestPerMegabyte, for: image.count))
        let expected = Insecure.MD5.hash(data: image).map { String(format: "%02x", $0) }.joined()
        guard String(decoding: digest, as: UTF8.self).lowercased() == expected else {
            throw Failure("The device holds something else than was written. Install again.")
        }
    }

    /// Leaves the bootloader for the application.
    func restart() async throws {
        // Entered from the application, the bootloader would come back after a reset.
        try writeRegister(Register.bootOption, 0, mask: Register.forceDownload)
        // ESP32-S3 TRM, table 33.4-4. Both lines low clear the USB download flag.
        try link.setModemLines(dtr: false, rts: false)
        try link.setModemLines(dtr: false, rts: true)
        try await pause(.milliseconds(100))
        try link.setModemLines(dtr: false, rts: false)
    }

    private func readRegister(_ address: UInt32) throws -> UInt32 {
        try exchange(.readRegister, Self.words(address), status: "Reading a register").value
    }

    private func writeRegister(_ address: UInt32, _ value: UInt32, mask: UInt32 = 0xFFFF_FFFF) throws {
        try check(.writeRegister, Self.words(address, value, mask, 0), "Writing a register")
    }

    /// A request that has to succeed. Returns the data of the reply.
    @discardableResult
    private func check(
        _ command: Command, _ data: [UInt8], _ doing: String, checksum: UInt32 = 0, replyBytes: Int = 0,
        within timeout: Duration = reply
    ) throws -> [UInt8] {
        try exchange(command, data, checksum: checksum, replyBytes: replyBytes, within: timeout, status: doing).data
    }

    /// Sends a request and waits for the reply to it. With a status the reply has to report success.
    private func exchange(
        _ command: Command, _ data: [UInt8], checksum: UInt32 = 0, replyBytes: Int = 0, within timeout: Duration = reply,
        status doing: String? = nil
    ) throws -> (value: UInt32, data: [UInt8]) {
        let header = [0x00, command.rawValue] + Self.bytes(UInt16(data.count)) + Self.words(checksum)
        try link.write(Slip.encode(header + data))
        let left = timeLeft(timeout)
        while true {
            guard let reply = try frame(within: left) else {
                throw Failure("\(doing ?? "The bootloader"): no answer within \(timeout.milliseconds) ms.")
            }
            // What answers another request is left over from before.
            guard reply.count >= 8, reply[0] == 0x01, reply[1] == command.rawValue else { continue }
            let value = reply[4..<8].enumerated().reduce(UInt32(0)) { $0 | UInt32($1.element) << (8 * UInt32($1.offset)) }
            let body = Array(reply[8...])
            if let doing {
                guard body.count >= replyBytes + 2 else {
                    throw Failure("\(doing): the answer is too short.")
                }
                if body[replyBytes] != 0 {
                    throw Failure("\(doing) failed: \(Self.reason(body[replyBytes + 1])).")
                }
            }
            return (value, Array(body.prefix(replyBytes)))
        }
    }

    /// The next frame, or nil when none is complete while time is left.
    private func frame(within left: () -> Duration) throws -> [UInt8]? {
        while true {
            if let frame = Slip.take(from: &received) {
                return frame
            }
            let time = left()
            guard time > .zero else { return nil }
            received += try link.read(atMost: 4096, within: time)
        }
    }

    private static func reason(_ code: UInt8) -> String {
        switch code {
        case 0x05: "the request was invalid"
        case 0x06: "the bootloader could not do it"
        case 0x07: "the checksum did not match"
        case 0x08: "the flash did not take the data"
        case 0x09: "the flash could not be read"
        case 0x0A: "the length was wrong"
        case 0x0B: "the data did not unpack"
        default: String(format: "error 0x%02X", code)
        }
    }

    private static func time(_ secondsPerMegabyte: Double, for bytes: Int) -> Duration {
        max(reply, .seconds(secondsPerMegabyte * Double(bytes) / 1e6))
    }

    private static func bytes<Value: FixedWidthInteger>(_ value: Value) -> [UInt8] {
        withUnsafeBytes(of: value.littleEndian) { Array($0) }
    }

    private static func words(_ values: UInt32...) -> [UInt8] {
        values.flatMap(bytes)
    }

    private static func compress(_ image: [UInt8]) throws -> [UInt8] {
        var length = compressBound(uLong(image.count))
        var packed = [UInt8](repeating: 0, count: Int(length))
        guard compress2(&packed, &length, image, uLong(image.count), Z_BEST_COMPRESSION) == Z_OK else {
            throw Failure("The image could not be compressed.")
        }
        return Array(packed.prefix(Int(length)))
    }
}

/// SLIP, the framing of the serial protocol: a frame between two END bytes, with END and ESC escaped.
enum Slip {
    private static let end: UInt8 = 0xC0, escape: UInt8 = 0xDB, escapedEnd: UInt8 = 0xDC, escapedEscape: UInt8 = 0xDD

    static func encode(_ frame: [UInt8]) -> [UInt8] {
        [end] + frame.flatMap { $0 == end ? [escape, escapedEnd] : $0 == escape ? [escape, escapedEscape] : [$0] } + [end]
    }

    /// Takes the first complete frame out of the bytes, with whatever precedes it. Nil while none is complete.
    static func take(from bytes: inout [UInt8]) -> [UInt8]? {
        guard let start = bytes.firstIndex(of: end) else {
            bytes = []
            return nil
        }
        // Two END bytes in a row are the end of one frame and the start of the next.
        var first = start + 1
        while first < bytes.count, bytes[first] == end { first += 1 }
        guard let last = bytes[first...].firstIndex(of: end) else {
            bytes.removeFirst(first - 1)
            return nil
        }
        var frame: [UInt8] = []
        var escaped = false
        for byte in bytes[first..<last] {
            if escaped {
                frame.append(byte == escapedEnd ? end : byte == escapedEscape ? escape : byte)
                escaped = false
            } else if byte == escape {
                escaped = true
            } else {
                frame.append(byte)
            }
        }
        // The END stays: it starts the next frame when the sender puts one END between two.
        bytes.removeFirst(last)
        return frame
    }
}

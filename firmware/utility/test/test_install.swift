import CryptoKit
import Foundation
import Synchronization
import zlib

/// Time that passes when a test says so.
final class ManualClock: Clock {
    struct Instant: InstantProtocol {
        var sinceStart: Duration
        func advanced(by duration: Duration) -> Instant { Instant(sinceStart: sinceStart + duration) }
        func duration(to other: Instant) -> Duration { other.sinceStart - sinceStart }
        static func < (earlier: Instant, later: Instant) -> Bool { earlier.sinceStart < later.sinceStart }
    }

    private let passed = Mutex(Duration.zero)
    var now: Instant { Instant(sinceStart: passed.withLock { $0 }) }
    var minimumResolution: Duration { .zero }

    func advance(by duration: Duration) {
        passed.withLock { $0 += duration }
    }

    func sleep(until deadline: Instant, tolerance: Duration?) async throws {
        passed.withLock { $0 = max($0, deadline.sinceStart) }
    }
}

final class FakeRom: SerialLink {
    let clock = ManualClock()
    var flash: [Int: UInt8] = [:]
    var registers: [UInt32: UInt32] = [0x3FCE_F14C: 4, 0x6000_812C: 1]
    var modemLines: [(dtr: Bool, rts: Bool)] = []
    var flashSize = 0

    var silentBlock: Int?
    var unplugAtBlock: Int?
    var corrupt = false

    private var incoming: [UInt8] = []
    private var outgoing: [UInt8] = []
    private var packed: [UInt8] = []
    private var offset = 0, size = 0, blocks = 0

    func write(_ bytes: [UInt8]) throws {
        incoming += bytes
        while let request = Slip.take(from: &incoming) {
            try answer(request)
        }
    }

    func read(atMost count: Int, within timeout: Duration) throws -> [UInt8] {
        let bytes = Array(outgoing.prefix(count))
        outgoing.removeFirst(bytes.count)
        if bytes.isEmpty {
            clock.advance(by: timeout)
        }
        return bytes
    }

    func discardInput() { outgoing = [] }
    func setModemLines(dtr: Bool, rts: Bool) { modemLines.append((dtr, rts)) }

    private func word(_ bytes: [UInt8], _ index: Int) -> UInt32 {
        (0..<4).reduce(0) { $0 | UInt32(bytes[index * 4 + $1]) << (8 * UInt32($1)) }
    }

    private func reply(_ command: UInt8, value: UInt32 = 0, data: [UInt8] = [], error: UInt8 = 0) {
        let body = data + [error == 0 ? 0 : 1, error, 0, 0]
        let header = [0x01, command, UInt8(body.count & 0xFF), UInt8(body.count >> 8)]
        outgoing += Slip.encode(header + (0..<4).map { UInt8(value >> (8 * UInt32($0)) & 0xFF) } + body)
    }

    private func answer(_ request: [UInt8]) throws {
        let command = request[1], data = Array(request[8...])
        guard Int(request[2]) | Int(request[3]) << 8 == data.count else {
            return reply(command, error: 0x05)
        }
        switch command {
        case 0x08:
            guard data == [0x07, 0x07, 0x12, 0x20] + [UInt8](repeating: 0x55, count: 32) else { return }
            for _ in 0..<8 { reply(command, value: 0x2000_0000) }
        case 0x0A:
            reply(command, value: registers[word(data, 0)] ?? 0)
        case 0x09:
            let address = word(data, 0), mask = word(data, 2)
            registers[address] = (registers[address] ?? 0) & ~mask | word(data, 1) & mask
            reply(command)
        case 0x0D:
            reply(command, error: data.count == 8 ? 0 : 0x05)
        case 0x0B:
            flashSize = Int(word(data, 1))
            reply(command)
        case 0x10:
            // Five words: the ROM of an ESP32-S3 also takes whether the data is to be encrypted.
            guard data.count == 20 else { return reply(command, error: 0x05) }
            size = Int(word(data, 0))
            blocks = Int(word(data, 1))
            offset = Int(word(data, 3))
            packed = []
            guard offset + size <= flashSize, offset % 4096 == 0 else { return reply(command, error: 0x06) }
            for address in offset..<offset + size { flash[address] = 0xFF }
            reply(command)
        case 0x11:
            let sequence = Int(word(data, 1)), block = Array(data[16...])
            if sequence == unplugAtBlock {
                throw Failure("The port closed: the device is gone.")
            }
            guard block.count == Int(word(data, 0)), block.count <= FlashUtil.blockSize, sequence < blocks else {
                return reply(command, error: 0x0A)
            }
            guard UInt32(block.reduce(0xEF, ^)) == word(request, 1) else { return reply(command, error: 0x07) }
            packed += block
            if sequence == blocks - 1 {
                var length = uLong(size)
                var image = [UInt8](repeating: 0, count: size)
                guard uncompress(&image, &length, packed, uLong(packed.count)) == Z_OK else {
                    return reply(command, error: 0x0B)
                }
                for (index, byte) in image.prefix(Int(length)).enumerated() { flash[offset + index] = byte }
                if corrupt { flash[offset]! ^= 1 }
            }
            if sequence != silentBlock { reply(command) }
        case 0x13:
            let from = Int(word(data, 0)), count = Int(word(data, 1))
            let digest = Insecure.MD5.hash(data: (from..<from + count).map { flash[$0] ?? 0xFF })
            reply(command, data: Array(digest.map { String(format: "%02x", $0) }.joined().utf8))
        default:
            reply(command, error: 0x05)
        }
    }
}

nonisolated(unsafe) var failures = 0

func check(_ condition: Bool, _ what: String, line: Int = #line) {
    if !condition {
        failures += 1
        print("FAILED line \(line): \(what)")
    }
}

func failure(_ body: () throws -> Void) -> String? {
    do {
        try body()
        return nil
    } catch {
        return error.localizedDescription
    }
}

let offset = 0x10000, flashSize = 16 * 1024 * 1024
let factory = offset..<0x810000
/// Mixed content of 150 KB: text that packs well, and noise that does not.
let image: [UInt8] = {
    var state: UInt32 = 2_463_534_242
    return (0..<150_001).map { index in
        state ^= state << 13
        state ^= state >> 17
        state ^= state << 5
        return index < 50_000 ? UInt8(index % 7) : UInt8(truncatingIfNeeded: state)
    }
}()

func testSlip() {
    let frame: [UInt8] = [0x00, 0xC0, 0xDB, 0xDC, 0x01]
    let wire: [UInt8] = [0xC0, 0x00, 0xDB, 0xDC, 0xDB, 0xDD, 0xDC, 0x01, 0xC0]
    check(Slip.encode(frame) == wire, "END and ESC have their ROM wire encodings")
    var bytes = [0x55] + wire + [0xC0, 0x02, 0xC0]
    check(Slip.take(from: &bytes) ?? [] == frame, "a frame with END and ESC in it comes back as it was")
    check(Slip.take(from: &bytes) ?? [] == [0x02], "the frame after it follows")
    check(Slip.take(from: &bytes) == nil, "then there is none")
    var half = Array(wire.dropLast(2))
    check(Slip.take(from: &half) == nil, "half a frame is none")
    half += [0x01, 0xC0]
    check(Slip.take(from: &half) ?? [] == frame, "and one once the rest arrived")
}

func testInstall() async {
    let rom = FakeRom()
    let flash = FlashUtil(rom, clock: rom.clock)
    var steps: [FlashUtil.Step] = []
    check(failure { try flash.connect() } == nil, "the ROM answers")
    check(rom.registers[0x6000_8098] == 0, "the watchdog is off")
    check((rom.registers[0x6000_80B4] ?? 0) >> 31 == 1, "the super watchdog feeds itself")
    check(failure { try flash.write(image, in: factory, flashSize: flashSize) { steps.append($0) } } == nil, "the image is written")
    check(rom.flashSize == flashSize, "the ROM knows the size of the flash")
    let padded = image + [0xFF, 0xFF, 0xFF]
    check((0..<padded.count).allSatisfy { rom.flash[offset + $0] == padded[$0] }, "the flash holds the image, padded to a word")
    check(rom.flash[offset - 1] == nil && rom.flash[offset + 151 * 1024] == nil, "nothing around it was touched")
    check(steps.first == .erasing && steps.last == .verifying, "the steps go from erasing to verifying")
    let shares = steps.compactMap { if case .writing(let share) = $0 { share } else { nil } }
    check(shares.first == 0 && shares == shares.sorted() && shares.allSatisfy { (0...1).contains($0) },
          "writing progress starts at zero and never goes backwards or exceeds one")
    let restarted: Void? = try? await flash.restart()
    check(restarted != nil, "the device restarts")
    check(rom.clock.now.sinceStart >= .milliseconds(100), "with the reset line held for a moment")
    check(rom.modemLines.map(\.rts) == [false, true, false] && rom.modemLines.allSatisfy { !$0.dtr },
          "the USB download flag clears before reset, with DTR low throughout")
    check(rom.registers[0x6000_812C] == 0, "after the bootloader was told to let go")
}

func testFailures() {
    let silent = FakeRom()
    silent.registers = [:]
    silent.silentBlock = 2
    var flash = FlashUtil(silent, clock: silent.clock)
    try? flash.connect()
    check(silent.registers[0x6000_8098] == nil, "a ROM on another port keeps its watchdogs")
    // A short image, as the wait for the answer grows with the size.
    let lost = failure { try flash.write(Array(image.suffix(4000)), in: factory, flashSize: flashSize) { _ in } } ?? ""
    check(lost.contains("Writing block 3") && lost.contains("no answer"), "a lost answer names the block: \(lost)")

    let corrupt = FakeRom()
    corrupt.corrupt = true
    flash = FlashUtil(corrupt, clock: corrupt.clock)
    try? flash.connect()
    let wrong = failure { try flash.write(image, in: factory, flashSize: flashSize) { _ in } } ?? ""
    check(wrong.contains("something else"), "a digest that differs fails the install: \(wrong)")

    let unplugged = FakeRom()
    unplugged.unplugAtBlock = 5
    flash = FlashUtil(unplugged, clock: unplugged.clock)
    try? flash.connect()
    let gone = failure { try flash.write(image, in: factory, flashSize: flashSize) { _ in } } ?? ""
    check(gone.contains("gone"), "a device that leaves says so: \(gone)")

    let small = FakeRom()
    flash = FlashUtil(small, clock: small.clock)
    try? flash.connect()
    let refused = failure { try flash.write(image, in: factory, flashSize: 0x20000) { _ in } } ?? ""
    check(refused.contains("partition") && small.flashSize == 0, "a partition outside flash fails before the erase: \(refused)")
    let oversized = failure { try flash.write(image, in: offset..<offset + 4096, flashSize: flashSize) { _ in } } ?? ""
    check(oversized.contains("partition") && small.flashSize == 0, "a write beyond its partition fails before the erase")

    final class Dead: SerialLink {
        let clock = ManualClock()
        func write(_ bytes: [UInt8]) throws {}
        func read(atMost count: Int, within timeout: Duration) throws -> [UInt8] {
            clock.advance(by: timeout)
            return []
        }
        func discardInput() {}
        func setModemLines(dtr: Bool, rts: Bool) {}
    }
    let dead = Dead()
    check(failure { try FlashUtil(dead, clock: dead.clock).connect() } == "The bootloader does not answer.", "a port without a ROM is told apart")
}

func testCancel() async {
    let duringConnect = Task.detached {
        final class Silent: SerialLink {
            let clock = ManualClock()
            var reads = 0
            func write(_ bytes: [UInt8]) throws {}
            func read(atMost count: Int, within timeout: Duration) throws -> [UInt8] {
                reads += 1
                clock.advance(by: timeout)
                if reads == 2 {
                    withUnsafeCurrentTask { $0?.cancel() }
                }
                return []
            }
            func discardInput() {}
            func setModemLines(dtr: Bool, rts: Bool) {}
        }
        let silent = Silent()
        do {
            try FlashUtil(silent, clock: silent.clock).connect()
            return false
        } catch is CancellationError {
            return silent.reads < 5
        } catch {
            return false
        }
    }
    check(await duringConnect.value, "cancelling during connect stops promptly")
    let beforeErase = Task.detached {
        let rom = FakeRom()
        let flash = FlashUtil(rom, clock: rom.clock)
        do {
            try flash.connect()
            withUnsafeCurrentTask { $0?.cancel() }
            try flash.write(image, in: factory, flashSize: flashSize) { _ in }
            return false
        } catch {
            return error is CancellationError && rom.flash.isEmpty
        }
    }
    let untouched = await beforeErase.value
    check(untouched, "cancelling before erase leaves flash untouched")
    let task = Task.detached {
        let rom = FakeRom()
        let flash = FlashUtil(rom, clock: rom.clock)
        do {
            try flash.connect()
            try flash.write(image, in: factory, flashSize: flashSize) { step in
                if case .writing(let share) = step, share > 0.5 {
                    withUnsafeCurrentTask { $0?.cancel() }
                }
            }
            return true
        } catch {
            return false
        }
    }
    let result = await task.value
    check(result, "cancelling during writing still finishes and verifies the image")
}

func testTerminal() {
    var terminal = TerminalText()
    check(terminal.feed(Array("I (12) main: \u{1B}[0;32mstarted\u{1B}[0m\r\n".utf8)).isEmpty, "a log line asks nothing")
    check(terminal.text == "I (12) main: started\n", "colours are dropped: \(terminal.text)")

    // The line editor asks for the width: where is the cursor, far right, where is it now, back.
    check(terminal.feed(Array("\u{1B}[6n".utf8)) == Array("\u{1B}[1;1R".utf8), "the cursor starts at column 1")
    let probe = Array("\u{1B}[999C\u{1B}[6n".utf8)
    for width in [120, 44, 180] {
        check(terminal.feed(probe, columns: width) == Array("\u{1B}[1;\(width)R".utf8), "reports the resized width")
    }
    // A device may send anything: a count beyond every number, a sequence without an end.
    check(terminal.feed(Array("\u{1B}[9223372036854775807C\u{1B}[6n".utf8)) == Array("\u{1B}[1;\(TerminalText.defaultColumns)R".utf8),
          "a huge count stops at the width")
    _ = terminal.feed(Array(("\u{1B}[" + String(repeating: "1;", count: 10_000) + "6n").utf8))
    _ = terminal.feed(Array("\u{1B}[\(TerminalText.defaultColumns - 1)D".utf8))

    // It writes the whole line again for every key.
    for typed in ["h", "he", "hel"] {
        _ = terminal.feed(Array("\rairgrep> \(typed)\u{1B}[0K\r\u{1B}[\(9 + typed.count)C".utf8))
    }
    check(terminal.text.hasSuffix("\nairgrep> hel"), "typing shows one line: \(terminal.text)")
    _ = terminal.feed(Array("\rairgrep> he\u{1B}[0K\r\u{1B}[11C".utf8))
    check(terminal.text.hasSuffix("\nairgrep> he"), "backspace takes a character off")
    _ = terminal.feed(Array("\r\nok ".utf8) + [0xC3])
    _ = terminal.feed([0xA4] + Array("\u{1B}[2".utf8))
    _ = terminal.feed(Array("K\rdone".utf8))
    check(terminal.text.hasSuffix("airgrep> he\ndone"), "a sequence split over two reads still counts: \(terminal.text)")
    _ = terminal.feed(Array("\rgr".utf8) + [0xC3])
    _ = terminal.feed([0xBC] + Array("n\u{1B}[0K".utf8))
    check(terminal.text.hasSuffix("\ngrün"), "and so does a character: \(terminal.text)")
    _ = terminal.feed(Array("\u{1B}[H\u{1B}[2J\rairgrep> \u{1B}[0K\r\u{1B}[9C".utf8))
    check(terminal.text.hasSuffix("\ngrün\nairgrep> "), "a cleared screen keeps what was printed and starts a line: \(terminal.text)")
    check(terminal.column == 9 && terminal.lines.last == "grün", "the cursor stands behind the prompt")
    let drops = terminal.drops
    terminal.clear()
    check(terminal.text.isEmpty && terminal.column == 0 && terminal.drops == drops + 1, "clear forgets the text and says the lines moved")

    // Clear resets incomplete escape and UTF-8 state, control bytes flush partial UTF-8, and huge or zero widths stay safe.
    _ = terminal.feed([0x1B, UInt8(ascii: "["), UInt8(ascii: "3")])
    terminal.clear()
    _ = terminal.feed(Array("plain".utf8))
    check(terminal.text == "plain", "clear resets pending escape sequence: \(terminal.text)")
    _ = terminal.feed([0xC3, UInt8(ascii: "\n")] + Array("next".utf8))
    check(terminal.text == "plain\u{FFFD}\nnext", "control byte flushes partial UTF-8: \(terminal.text)")
    _ = terminal.feed(Array("\u{1B}[99999999999999999999C\u{1B}[6n".utf8), columns: 40)
    check(terminal.column == 39, "overflowing count clamps to width: \(terminal.column)")
    _ = terminal.feed(Array("\u{1B}[1\u{1B}[2K\rhi".utf8), columns: 0)
    check(terminal.text.hasSuffix("\nhi"), "nested ESC and zero width stay safe: \(terminal.text)")
}

@main
enum Tests {
    static func main() async {
        testSlip()
        await testInstall()
        testFailures()
        testImages()
        testPresence()
        await testCancel()
        testTerminal()
        testReadout()
        print(failures == 0 ? "ok" : "\(failures) failed")
        exit(failures == 0 ? 0 : 1)
    }
}

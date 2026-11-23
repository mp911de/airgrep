import Foundation

/// The console of the demo device: a prompt, a line that echoes what is typed, and a few commands.
/// It speaks as the line editor of the firmware does, so the console window needs no other path.
final class EmulatedConsole: SerialLink, @unchecked Sendable {
    private static let prompt = "airgrep> "
    private static let commands = [
        "help": "help      this list\r\nversion   the firmware of the device\r\n",
        "version": "airgrep demo device, no firmware\r\n",
    ]

    private let arrived = NSCondition()
    private var output: [UInt8] = []
    private var line = ""

    func write(_ bytes: [UInt8]) {
        var answer = ""
        for byte in bytes {
            switch byte {
            // Control+L: the prompt and the line once more.
            case 0x0C:
                answer += "\r\u{1B}[K" + Self.prompt + line
            case 0x0D, 0x0A:
                let command = line.trimmingCharacters(in: .whitespaces)
                line = ""
                answer += "\r\n"
                if !command.isEmpty {
                    answer += Self.commands[command] ?? "Unknown command: \(command). Try help.\r\n"
                }
                answer += Self.prompt
            case 0x08, 0x7F:
                if !line.isEmpty {
                    line.removeLast()
                    answer += "\u{1B}[D\u{1B}[K"
                }
            case 0x20..<0x7F:
                let character = Character(UnicodeScalar(byte))
                line.append(character)
                answer.append(character)
            default:
                break
            }
        }
        arrived.lock()
        output += Array(answer.utf8)
        arrived.signal()
        arrived.unlock()
    }

    func read(atMost count: Int, within timeout: Duration) -> [UInt8] {
        arrived.lock()
        defer { arrived.unlock() }
        let deadline = Date.now + Double(timeout.components.seconds)
            + Double(timeout.components.attoseconds) / 1e18
        while output.isEmpty, arrived.wait(until: deadline) {}
        let bytes = Array(output.prefix(count))
        output.removeFirst(bytes.count)
        return bytes
    }

    func discardInput() {
        arrived.lock()
        output = []
        arrived.unlock()
    }

    func setModemLines(dtr: Bool, rts: Bool) {}
}

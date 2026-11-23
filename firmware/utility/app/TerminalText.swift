/// What a terminal shows of the bytes a console sends: the text, with the line under the cursor
/// edited by the escape sequences of the firmware's line editor. Colours are dropped.
struct TerminalText {
    static let defaultColumns = 120
    /// A control sequence has a few numbers. What an endless one sends beyond this is dropped.
    static let parametersKept = 32
    /// Finished lines beyond this are dropped from the top, a part of them at a time.
    static let kept = 5000, dropped = 1000

    /// The lines the cursor has left, oldest first.
    private(set) var lines: [String] = []
    /// Counts the times lines were dropped, after which a line is not where it was.
    private(set) var drops = 0
    /// The line under the cursor, and the cursor on it.
    private(set) var line: [Character] = []
    private(set) var column = 0
    private var state = State.text
    private var parameters = ""
    /// Bytes of a character that is not complete yet.
    private var partial: [UInt8] = []

    private enum State { case text, escape, sequence }

    var text: String { lines.map { $0 + "\n" }.joined() + String(line) }

    /// Forgets everything printed. A line is not where it was, as after a drop.
    mutating func clear() {
        lines = []
        line = []
        column = 0
        drops += 1
    }

    /// Takes what the device sent and returns what a terminal answers, which is empty unless it was asked.
    mutating func feed(_ bytes: [UInt8], columns: Int = Self.defaultColumns) -> [UInt8] {
        var answer: [UInt8] = []
        for byte in bytes {
            switch state {
            case .escape:
                state = byte == UInt8(ascii: "[") ? .sequence : .text
                parameters = ""
            case .sequence:
                if (0x40...0x7E).contains(byte) {
                    answer += control(Character(UnicodeScalar(byte)), columns: columns)
                    state = .text
                } else if parameters.count < Self.parametersKept {
                    parameters.append(Character(UnicodeScalar(byte)))
                }
            case .text:
                plain(byte)
            }
        }
        return answer
    }

    private mutating func plain(_ byte: UInt8) {
        switch byte {
        case 0x1B:
            state = .escape
        case UInt8(ascii: "\r"):
            column = 0
        case UInt8(ascii: "\n"):
            leaveLine()
        case 0x08:
            column = max(0, column - 1)
        case UInt8(ascii: "\t"):
            column = (column / 8 + 1) * 8
        case 0x20...:
            partial.append(byte)
            // A character of UTF-8 is complete when its bytes decode without a replacement.
            let decoded = String(decoding: partial, as: UTF8.self)
            if byte < 0x80 || !decoded.unicodeScalars.contains("\u{FFFD}") || partial.count >= 4 {
                for character in decoded { put(character) }
                partial = []
            }
        default:
            break
        }
    }

    private mutating func leaveLine() {
        lines.append(String(line))
        line = []
        column = 0
        if lines.count > Self.kept {
            lines.removeFirst(Self.dropped)
            drops += 1
        }
    }

    private mutating func put(_ character: Character) {
        if column > line.count {
            line += [Character](repeating: " ", count: column - line.count)
        }
        if column < line.count {
            line[column] = character
        } else {
            line.append(character)
        }
        column += 1
    }

    /// A control sequence by its last character, with the numbers before it.
    private mutating func control(_ final: Character, columns: Int) -> [UInt8] {
        // No number of a sequence means more than the width, and a larger one must not overflow.
        let numbers = parameters.split(separator: ";").map { min(Int($0) ?? 0, columns) }
        let count = max(numbers.first ?? 1, 1)
        switch final {
        case "C": column = min(column + count, columns - 1)
        case "D": column = max(column - count, 0)
        case "G": column = min(count, columns) - 1
        case "K":
            switch numbers.first ?? 0 {
            case 0: line.removeSubrange(min(column, line.count)...)
            case 1: line.replaceSubrange(..<min(column, line.count), with: [Character](repeating: " ", count: min(column, line.count)))
            default: line = []
            }
        case "J" where numbers.first == 2:
            // A cleared screen keeps what scrolled off it. Here everything printed has.
            if !line.isEmpty { leaveLine() }
        case "n" where numbers.first == 6:
            // The line editor learns the width by moving far right and asking where the cursor is.
            return Array("\u{1B}[1;\(column + 1)R".utf8)
        default: break
        }
        return []
    }
}

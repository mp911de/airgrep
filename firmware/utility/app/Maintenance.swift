import CryptoKit
import Foundation

/// What can be installed on a device, and where the partition table puts it.
enum Maintenance {
    enum Kind: CaseIterable, Sendable {
        case firmware, timeZones

        var buttonTitle: String {
            switch self {
            case .firmware: "Firmware…"
            case .timeZones: "Time Zone Data…"
            }
        }

        var filePrompt: String {
            switch self {
            case .firmware: "Choose a firmware image, airgrep.bin"
            case .timeZones: "Choose a time zone table, tzdata.bin"
            }
        }

        var partitionName: String {
            switch self {
            case .firmware: "factory"
            case .timeZones: "tzdata"
            }
        }
    }

    /// A file that was looked at and belongs where it is about to be written.
    struct Image: Sendable {
        let file: URL
        let kind: Kind
        let version: String
        let bytes: [UInt8]
        let partition: Range<Int>
        /// The size of the flash the partition table was made for.
        let flashSize: Int
        /// What the file says about itself, for the user to read before a yes.
        var summary: String { "Version: \(version)\nSize: \(bytes.count / 1024) KB" }

        /// ESP application image: magic byte, then the application description with its own magic.
        private static let imageMagic: UInt8 = 0xE9
        private static let descriptionAt = 32, descriptionMagic: [UInt8] = [0x32, 0x54, 0xCD, 0xAB]
        private static let versionAt = 48, projectAt = 80, textSize = 32
        private static let project = "airgrep"
        private static let tableMagic = Array("TZL2".utf8), tableSourceAt = 64, tableHeader = 128

        /// What a file is by its first bytes, or nil for one this application does not install.
        static func kind(of file: URL) -> Kind? {
            guard let handle = try? FileHandle(forReadingFrom: file) else { return nil }
            defer { try? handle.close() }
            let head = [UInt8]((try? handle.read(upToCount: descriptionAt + 4)) ?? Data())
            if head.starts(with: tableMagic) { return .timeZones }
            if head.first == imageMagic, head.dropFirst(descriptionAt).elementsEqual(descriptionMagic) { return .firmware }
            return nil
        }

        /// A file that says itself what it is, as one dropped on the application.
        init(_ file: URL, board: String, partitionsFile: URL? = Bundle.main.url(forResource: "partitions", withExtension: "csv")) throws {
            guard let kind = Self.kind(of: file) else {
                throw Failure("\(file.lastPathComponent) is neither airgrep firmware nor a time zone table.")
            }
            try self.init(file, kind, board: board, partitionsFile: partitionsFile)
        }

        init(_ file: URL, _ kind: Kind, board: String, partitionsFile: URL? = Bundle.main.url(forResource: "partitions", withExtension: "csv")) throws {
            let partitions = try Maintenance.partitions(partitionsFile)
            guard let partition = partitions[kind.partitionName] else {
                throw Failure("The partition table of this application has no partition for \(file.lastPathComponent).")
            }
            let name = file.lastPathComponent
            guard let handle = try? FileHandle(forReadingFrom: file) else {
                throw Failure("\(name) cannot be read.")
            }
            defer { try? handle.close() }
            var contents = [UInt8]()
            while contents.count <= partition.size {
                let count = min(1024 * 1024, partition.size + 1 - contents.count)
                guard let part = try handle.read(upToCount: count), !part.isEmpty else { break }
                contents.append(contentsOf: part)
            }
            let bytes = contents, size = bytes.count
            guard size <= partition.size else {
                throw Failure("\(name) exceeds its \(partition.size) byte partition.")
            }
            func text(_ at: Int, _ count: Int) -> String {
                guard bytes.count >= at + count else { return "" }
                return String(decoding: bytes[at..<at + count].prefix { $0 != 0 }, as: UTF8.self)
            }
            switch kind {
            case .firmware:
                guard bytes.first == Self.imageMagic,
                      bytes.count >= Self.projectAt + Self.textSize,
                      Array(bytes[Self.descriptionAt..<Self.descriptionAt + 4]) == Self.descriptionMagic
                else {
                    throw Failure("\(name) is not an ESP application image.")
                }
                guard text(Self.projectAt, Self.textSize) == Self.project else {
                    throw Failure("\(name) is firmware of another project, \(text(Self.projectAt, Self.textSize)).")
                }
                try Self.checkFirmware(bytes, board: board)
                version = text(Self.versionAt, Self.textSize)
            case .timeZones:
                guard bytes.count >= Self.tableHeader, Array(bytes[0..<4]) == Self.tableMagic,
                      Self.word(bytes, at: 40) == size else {
                    throw Failure("\(name) is not a TZL2 time zone table.")
                }
                version = String(text(Self.tableSourceAt, 64).prefix { $0 != "/" })
            }
            self.file = file
            self.kind = kind
            self.bytes = bytes
            self.partition = partition.offset..<partition.offset + partition.size
            // Flash comes in powers of two, and the table ends where the flash does or before.
            let end = partitions.values.map { $0.offset + $0.size }.max() ?? 0
            flashSize = 1 << Int(log2(Double(max(end, 1))).rounded(.up))
        }

        private static func word(_ bytes: [UInt8], at: Int) -> Int {
            Int(bytes[at]) | Int(bytes[at + 1]) << 8 | Int(bytes[at + 2]) << 16 | Int(bytes[at + 3]) << 24
        }

        private static func checkFirmware(_ bytes: [UInt8], board: String) throws {
            guard bytes[12] == 9, bytes[13] == 0 else { throw Failure("The firmware is for another chip.") }
            guard (1...16).contains(bytes[1]), bytes[23] <= 1 else { throw Failure("Invalid ESP image header.") }
            let boardAt = descriptionAt + 256
            let descriptor = Array("airgrep-board:\(board)".utf8) + [0]
            var cursor = 24
            var checksum: UInt8 = 0xEF
            for segment in 0..<Int(bytes[1]) {
                guard bytes.count - cursor >= 8 else { throw Failure("Incomplete ESP segment header.") }
                let length = word(bytes, at: cursor + 4)
                cursor += 8
                guard length <= bytes.count - cursor else { throw Failure("Incomplete ESP segment.") }
                let end = cursor + length
                if segment == 0 {
                    guard !board.isEmpty, end >= boardAt + descriptor.count,
                          bytes[boardAt..<end].starts(with: descriptor) else {
                        throw Failure("The firmware does not declare the connected board profile, \(board).")
                    }
                }
                checksum = bytes[cursor..<end].reduce(checksum, ^)
                cursor = end
            }
            // ESP images put the checksum in the final byte of a 16 byte block.
            let checksumAt = cursor | 15
            let end = checksumAt + 1, hasDigest = bytes[23] == 1
            guard bytes.count == end + (hasDigest ? 32 : 0), bytes[checksumAt] == checksum else {
                throw Failure("The firmware length or checksum is invalid.")
            }
            if hasDigest, !SHA256.hash(data: bytes[..<end]).elementsEqual(bytes[end...]) {
                throw Failure("The firmware SHA256 digest is invalid.")
            }
        }
    }

    /// The table the application was built with, partitions.csv in its resources, by partition name.
    private static func partitions(_ file: URL?) throws -> [String: (offset: Int, size: Int)] {
        func number(_ text: String) -> Int? {
            let lowered = text.lowercased()
            return lowered.hasPrefix("0x") ? Int(lowered.dropFirst(2), radix: 16) : Int(text)
        }
        guard let file,
              let table = try? String(contentsOf: file, encoding: .utf8)
        else {
            throw Failure("This application has no partition table.")
        }
        var partitions: [String: (offset: Int, size: Int)] = [:]
        for raw in table.split(whereSeparator: \.isNewline) {
            let line = raw.trimmingCharacters(in: .whitespaces)
            guard !line.isEmpty, !line.hasPrefix("#") else { continue }
            let fields = line.split(separator: ",", omittingEmptySubsequences: false)
                .map { $0.trimmingCharacters(in: .whitespaces) }
            if fields.count >= 5, !fields[0].isEmpty,
               let offset = number(fields[3]), offset >= 0,
               let size = number(fields[4]), size > 0, offset <= Int.max - size {
                partitions[fields[0]] = (offset, size)
            }
        }
        return partitions
    }
}

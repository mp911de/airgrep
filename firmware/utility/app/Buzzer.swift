import AppKit

/// The buzzer of the device, on this computer: its "short" scheme, 50 ms at 4 kHz.
@MainActor
final class Buzzer {
    static let shared = Buzzer()

    private static let frequency = 4000.0, seconds = 0.05, rate = 44100.0
    /// Quiet, and faded at both ends so it does not crack.
    private static let volume = 0.15, fade = 0.004

    private let sound: NSSound?

    private init() {
        let frames = Int(Self.rate * Self.seconds)
        var samples = Data(capacity: frames * 2)
        for frame in 0..<frames {
            let edge = min(Double(frame), Double(frames - 1 - frame)) / (Self.rate * Self.fade)
            let level = Self.volume * min(edge, 1) * sin(2 * Double.pi * Self.frequency * Double(frame) / Self.rate)
            withUnsafeBytes(of: Int16(level * Double(Int16.max)).littleEndian) { samples.append(contentsOf: $0) }
        }
        // A WAV file in memory: one channel of 16 bit samples.
        var file = Data()
        func append<Value: FixedWidthInteger>(_ value: Value) {
            withUnsafeBytes(of: value.littleEndian) { file.append(contentsOf: $0) }
        }
        file.append(contentsOf: Array("RIFF".utf8))
        append(UInt32(36 + samples.count))
        file.append(contentsOf: Array("WAVEfmt ".utf8))
        append(UInt32(16))
        append(UInt16(1))
        append(UInt16(1))
        append(UInt32(Self.rate))
        append(UInt32(Self.rate) * 2)
        append(UInt16(2))
        append(UInt16(16))
        file.append(contentsOf: Array("data".utf8))
        append(UInt32(samples.count))
        file.append(samples)
        sound = NSSound(data: file)
    }

    func click() {
        sound?.stop()
        sound?.play()
    }
}

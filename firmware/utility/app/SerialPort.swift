import Foundation
import IOKit
import IOKit.serial

/// Bytes to and from a device, and its modem control lines.
protocol SerialLink: AnyObject {
    func write(_ bytes: [UInt8]) throws
    /// What arrived within the time, at most count bytes. Empty when nothing did.
    func read(atMost count: Int, within timeout: Duration) throws -> [UInt8]
    func discardInput()
    func setModemLines(dtr: Bool, rts: Bool) throws
}

/// A serial port of this computer, open for as long as the object lives.
final class SerialPort: SerialLink, @unchecked Sendable {
    struct Failure: LocalizedError {
        let errorDescription: String?
    }

    let path: String
    private let descriptor: Int32

    // TIOCMSET in <sys/ttycom.h> is a macro Swift does not import.
    private static let setModemBits: UInt = 0x8004_746D
    private static let writeTimeout = Duration.seconds(10)

    init(path: String) throws {
        self.path = path
        // Without O_NONBLOCK opening waits for a carrier that a USB port never raises.
        descriptor = open(path, O_RDWR | O_NOCTTY | O_NONBLOCK)
        guard descriptor >= 0 else {
            throw Failure(errorDescription: "\(path) cannot be opened: \(Self.reason()).")
        }
        var settings = termios()
        guard tcgetattr(descriptor, &settings) == 0 else {
            let reason = Self.reason()
            close(descriptor)
            throw Failure(errorDescription: "\(path) is not a serial port: \(reason).")
        }
        cfmakeraw(&settings)
        cfsetspeed(&settings, speed_t(B115200))
        settings.c_cflag |= tcflag_t(CLOCAL | CREAD)
        tcsetattr(descriptor, TCSANOW, &settings)
    }

    deinit { close(descriptor) }

    func write(_ bytes: [UInt8]) throws {
        let deadline = ContinuousClock.now + Self.writeTimeout
        var sent = 0
        while sent < bytes.count {
            let count = bytes[sent...].withUnsafeBytes { Darwin.write(descriptor, $0.baseAddress, $0.count) }
            if count > 0 {
                sent += count
            } else if errno == EAGAIN, ContinuousClock.now < deadline {
                try wait(for: Int16(POLLOUT), within: .milliseconds(100))
            } else {
                throw Failure(errorDescription: "Writing to \(path) failed: \(Self.reason()).")
            }
        }
    }

    func read(atMost count: Int, within timeout: Duration) throws -> [UInt8] {
        guard try wait(for: Int16(POLLIN), within: timeout) else { return [] }
        var bytes = [UInt8](repeating: 0, count: count)
        let received = Darwin.read(descriptor, &bytes, count)
        if received > 0 {
            return Array(bytes.prefix(received))
        }
        if received < 0, errno == EAGAIN {
            return []
        }
        // A port that is readable and has nothing is one whose device left.
        throw Failure(errorDescription: "\(path) closed: \(received == 0 ? "the device is gone" : Self.reason()).")
    }

    func discardInput() {
        tcflush(descriptor, TCIFLUSH)
    }

    func setModemLines(dtr: Bool, rts: Bool) throws {
        var bits = (dtr ? TIOCM_DTR : 0) | (rts ? TIOCM_RTS : 0)
        guard ioctl(descriptor, Self.setModemBits, &bits) == 0 else {
            throw Failure(errorDescription: "\(path) does not take a reset: \(Self.reason()).")
        }
    }

    @discardableResult
    private func wait(for events: Int16, within timeout: Duration) throws -> Bool {
        var waiting = pollfd(fd: descriptor, events: events, revents: 0)
        let ready = poll(&waiting, 1, Int32(clamping: timeout.milliseconds))
        if ready < 0, errno != EINTR {
            throw Failure(errorDescription: "\(path) failed: \(Self.reason()).")
        }
        if waiting.revents & Int16(POLLNVAL) != 0 {
            throw Failure(errorDescription: "\(path) closed: the device is gone.")
        }
        return ready > 0
    }

    private static func reason() -> String { String(cString: strerror(errno)) }

    /// The serial port of the USB device with this identity, or nil while this computer shows none.
    /// macOS names the port of a bootloader after the place of the device on the bus, so the port is
    /// found by what the device says about itself.
    static func path(vendor: Int, product: Int, serial: String) -> String? {
        var ports: io_iterator_t = 0
        guard IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching(kIOSerialBSDServiceValue), &ports) == KERN_SUCCESS
        else { return nil }
        defer { IOObjectRelease(ports) }
        while case let port = IOIteratorNext(ports), port != 0 {
            defer { IOObjectRelease(port) }
            func property<Value>(_ key: String, ofParents parents: Bool) -> Value? {
                let options = parents ? IOOptionBits(kIORegistryIterateRecursively | kIORegistryIterateParents) : 0
                return IORegistryEntrySearchCFProperty(port, kIOServicePlane, key as CFString, kCFAllocatorDefault, options) as? Value
            }
            let number: String = property("USB Serial Number", ofParents: true) ?? ""
            if property("idVendor", ofParents: true) == vendor, property("idProduct", ofParents: true) == product,
               number.lowercased().filter(\.isHexDigit) == serial,
               let path: String = property(kIOCalloutDeviceKey, ofParents: false) {
                return path
            }
        }
        return nil
    }
}

extension Duration {
    var milliseconds: Int64 {
        components.seconds * 1000 + components.attoseconds / 1_000_000_000_000_000
    }
}

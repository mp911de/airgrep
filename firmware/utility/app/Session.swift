import SwiftUI

/// What a device said about itself, as the user interface shows it.
struct Details: Equatable {
    enum Phase { case connecting, connected, ended }

    var phase = Phase.connecting
    var error = ""
    var isReadable = false
    var uptime: UInt64 = 0
    var firmware = "", board = "", chip = ""
    var health: UInt16 = 0
    var healthText = ""
    var mode = ""
    /// Percent of core 0, core 1 and both, or nil when the firmware does not measure it.
    var load: [Int?] = []
    var internalFree: UInt32 = 0, internalMinimum: UInt32 = 0, internalTotal: UInt32 = 0
    var psramFree: UInt32 = 0, psramMinimum: UInt32 = 0, psramTotal: UInt32 = 0
    var pictures: UInt64 = 0
    var readMilliseconds = 0.0

    init() {}

    init(_ status: airgrep_status) {
        phase = status.phase == AIRGREP_CONNECTED ? .connected
            : status.phase == AIRGREP_ENDED ? .ended : .connecting
        error = text(status.error)
        isReadable = status.readable
        uptime = status.uptime_seconds
        firmware = text(status.firmware)
        board = text(status.board)
        chip = text(status.chip)
        health = status.health
        healthText = text(status.health_names)
        mode = text(status.mode)
        load = [status.cpu_core0, status.cpu_core1, status.cpu_overall].map { $0 == AIRGREP_CPU_UNAVAILABLE ? nil : Int($0) }
        internalFree = status.internal_free
        internalMinimum = status.internal_minimum
        internalTotal = status.internal_total
        psramFree = status.psram_free
        psramMinimum = status.psram_minimum
        psramTotal = status.psram_total
        pictures = status.pictures_read
        readMilliseconds = status.last_picture_read_ms
    }
}

/// The conversation with one device, from Connect until it ends or is closed.
@MainActor
final class Session: ObservableObject {
    let device: Device
    @Published private(set) var details = Details()
    /// A Remote Display window shows the device, so pictures of it are read.
    @Published private(set) var watching = false
    /// What the user asked the device to do that ends the conversation.
    @Published private(set) var asked: Ending?
    /// The install the page of the device shows, until the user has read how it ended.
    @Published var installation: Installation?

    enum Ending { case restart, bootloader }

    var isHeld: Bool { installation != nil || readers > 0 }

    /// The connection of the C interface, nil once closed. It never leaves this class.
    private var connection: OpaquePointer?
    private var timer: Timer?
    /// The windows that need pictures of the device.
    private var readers = 0
    /// Room for the data text, kept so a picture is turned into text once.
    private var dataBytes = [CChar](repeating: 0, count: 256 * 1024)

    init(_ device: Device) {
        self.device = device
        connection = airgrep_connect(device.serial)
        timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.refresh() }
        }
    }

    private func refresh() {
        guard let connection else { return }
        var status = airgrep_status()
        airgrep_status_of(connection, &status)
        let current = Details(status)
        if current != details {
            details = current
        }
    }

    /// Reads a picture every interval for a window that opened. Pictures are read while one needs them.
    func startReading(every interval: Duration) {
        guard let connection else { return }
        readers += 1
        airgrep_watch(connection, UInt32(interval.milliseconds))
    }

    func stopReading() {
        guard readers > 0 else { return }
        readers -= 1
        if readers == 0, let connection {
            airgrep_watch(connection, 0)
        }
    }

    /// The same for the display window, which the rest of the user interface knows about.
    func watch(every interval: Duration) {
        guard connection != nil, !watching else { return }
        watching = true
        startReading(every: interval)
    }

    func unwatch() {
        guard watching else { return }
        watching = false
        stopReading()
    }

    /// A new interval for the windows that are open.
    func rewatch(every interval: Duration) {
        if readers > 0, let connection {
            airgrep_watch(connection, UInt32(interval.milliseconds))
        }
    }

    /// The newest picture as text, headed by what a display made of it when one shows this device.
    func dataText(shownBy library: DisplayLibrary?) -> String {
        guard let connection else { return "" }
        var length = airgrep_data_text(connection, &dataBytes, dataBytes.count)
        guard length > 0 else { return "No picture of the device yet." }
        if length >= dataBytes.count {
            dataBytes = [CChar](repeating: 0, count: length * 2)
            length = airgrep_data_text(connection, &dataBytes, dataBytes.count)
        }
        let shown = library.flatMap { $0.api.explained() }.map { String(cString: $0) } ?? ""
        return (shown.isEmpty ? "" : shown + "\n") + text(dataBytes)
    }

    /// Lets a display show what was read since it last asked.
    func follow(on library: DisplayLibrary) {
        guard let connection else { return }
        device.serial.withCString { library.api.follow(connection, $0) }
    }

    func reboot() {
        guard let connection else { return }
        asked = .restart
        airgrep_reboot(connection)
    }

    func enterBootloader() {
        guard let connection else { return }
        asked = .bootloader
        airgrep_enter_bootloader(connection)
    }

    func resetSettings() {
        guard let connection else { return }
        asked = .restart
        airgrep_reset_settings(connection)
    }

    /// Ends the conversation. It waits for a request that is on its way, so it leaves the main thread.
    func close() {
        timer?.invalidate()
        watching = false
        readers = 0
        guard let connection else { return }
        nonisolated(unsafe) let closing = connection
        self.connection = nil
        DispatchQueue.global(qos: .utility).async {
            airgrep_disconnect(closing)
        }
    }
}

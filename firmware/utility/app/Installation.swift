import SwiftUI

/// One install on one device: from its bootloader showing up on a serial port to its restart.
@MainActor
final class Installation: ObservableObject, Identifiable {
    enum Phase: Equatable {
        case waiting, connecting, erasing
        case writing(Double)
        case verifying, upgraded, restarting
        /// How it ended otherwise, and what that means for the device.
        case cancelled(String), failed(String), rebootFailed(String)
    }

    let image: Maintenance.Image
    let device: Device
    @Published private(set) var phase = Phase.waiting {
        didSet { DockStatus.show(progress: written) }
    }

    /// How far the write is, while there is one. The sheet tells how an install ended.
    private var written: Double? {
        switch phase {
        case .waiting, .connecting, .erasing: 0
        case .writing(let share): share
        case .verifying: 1
        case .restarting, .upgraded, .cancelled, .failed, .rebootFailed: nil
        }
    }

    private var work: Task<SerialPort?, Never>?
    private var rebootPort: SerialPort?

    /// The USB identity of the ROM bootloader of an ESP32-S3, whose serial number is the factory MAC.
    nonisolated private static let bootloader = (vendor: 0x303A, product: 0x1001)
    /// A device that restarts into its bootloader is back on the bus within a few seconds.
    nonisolated private static let appears = Duration.seconds(20)

    init(_ image: Maintenance.Image, on device: Device) {
        self.image = image
        self.device = device
    }

    var running: Bool {
        switch phase {
        case .upgraded, .cancelled, .failed, .rebootFailed: false
        default: true
        }
    }

    /// Writes the image to the device, which is in its bootloader or on its way there.
    func start() {
        phase = .waiting
        let (phases, reporter) = AsyncStream.makeStream(of: Phase.self)
        let image = image, serial = device.serial
        let write = Task.detached(priority: .userInitiated) { () -> SerialPort? in
            defer { reporter.finish() }
            var touched = false
            /// What the device is left with when the install stops here.
            var left: String {
                touched
                    ? "What the device held there is incomplete. It stays in its bootloader, install again."
                    : "Nothing was written. The device stays in its bootloader until it is power cycled."
            }
            do {
                let port = try Self.install(image, on: serial) { phase in
                    touched = touched || phase == .erasing
                    reporter.yield(phase)
                }
                return port
            } catch is CancellationError {
                reporter.yield(.cancelled(left))
            } catch {
                reporter.yield(.failed("\(error.localizedDescription)\n\(left)"))
            }
            return nil
        }
        work = write
        Task {
            for await phase in phases {
                self.phase = phase
            }
            let port = await write.value
            work = nil
            if let port {
                rebootPort = port
                phase = .upgraded
            }
        }
    }

    var canCancel: Bool { phase == .waiting || phase == .connecting }

    func cancel() {
        guard canCancel else { return }
        work?.cancel()
    }

    func reboot(onSuccess: @escaping () -> Void) {
        guard let port = rebootPort else { return }
        rebootPort = nil
        phase = .restarting
        Task {
            do {
                try await Task.detached { try await FlashUtil(port, clock: ContinuousClock()).restart() }.value
                onSuccess()
            } catch {
                phase = .rebootFailed(error.localizedDescription)
            }
        }
    }

    nonisolated private static func install(_ image: Maintenance.Image, on serial: String, report: (Phase) -> Void) throws -> SerialPort {
        let deadline = ContinuousClock.now + appears
        let path: String
        while true {
            try Task.checkCancellation()
            if let found = SerialPort.path(vendor: bootloader.vendor, product: bootloader.product, serial: serial) {
                path = found
                break
            }
            guard ContinuousClock.now < deadline else {
                throw Failure("The bootloader of the device did not appear.")
            }
            Thread.sleep(forTimeInterval: 0.25)
        }
        report(.connecting)
        let port = try SerialPort(path: path)
        let flash = FlashUtil(port, clock: ContinuousClock())
        try flash.connect()
        try flash.write(image.bytes, in: image.partition, flashSize: image.flashSize) { step in
            switch step {
            case .erasing: report(.erasing)
            case .writing(let share): report(.writing(share))
            case .verifying: report(.verifying)
            }
        }
        return port
    }
}

/// How an install goes, for as long as it takes and until the user has read how it ended.
struct InstallSheet: View {
    @ObservedObject var installation: Installation
    let close: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            Text("\(installation.image.file.lastPathComponent) on \(installation.device.name)")
                .font(.headline)
            switch installation.phase {
            case .upgraded:
                Label("Upgrade finished.", systemImage: "checkmark.circle.fill")
            case .rebootFailed(let reason):
                ended("Installed, but reboot failed.\n\(reason)", systemImage: "exclamationmark.triangle.fill")
            case .cancelled(let left):
                ended("Cancelled.\n\(left)", systemImage: "xmark.circle")
            case .failed(let reason):
                ended(reason, systemImage: "exclamationmark.triangle.fill")
            case .writing(let share):
                ProgressView(value: share) { Text(step) }
            default:
                ProgressView { Text(step) }.progressViewStyle(.linear)
            }
            HStack {
                Spacer()
                if installation.running {
                    Button("Cancel", role: .cancel) { installation.cancel() }
                        .keyboardShortcut(.cancelAction)
                        .disabled(!installation.canCancel)
                } else if installation.phase == .upgraded {
                    Button("Reboot") { installation.reboot(onSuccess: close) }.keyboardShortcut(.defaultAction)
                } else if case .rebootFailed = installation.phase {
                    Button("Close", action: close).keyboardShortcut(.cancelAction)
                } else {
                    Button("Close", action: close).keyboardShortcut(.cancelAction)
                    Button("Install Again") { installation.start() }.keyboardShortcut(.defaultAction)
                }
            }
        }
        .padding(20)
        .frame(width: 440)
        .interactiveDismissDisabled()
    }

    private func ended(_ text: String, systemImage: String) -> some View {
        Label(text, systemImage: systemImage)
            .textSelection(.enabled)
            .fixedSize(horizontal: false, vertical: true)
    }

    private var step: String {
        switch installation.phase {
        case .waiting: "Waiting for the bootloader"
        case .connecting: "Connecting"
        case .erasing: "Erasing"
        case .writing(let share): "Writing, \(Int(share * 100)) %"
        case .verifying: "Verifying"
        case .restarting: "Restarting"
        case .upgraded, .cancelled, .failed, .rebootFailed: ""
        }
    }
}

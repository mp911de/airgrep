import SwiftUI
import UniformTypeIdentifiers

struct BridgeIdentity: Sendable {
    let mac: String
    let version: String
    let variant: String
    let factory: Range<Int>
    let tzdata: Range<Int>

    init?(_ line: String) {
        let fields = line.split(separator: "\t", omittingEmptySubsequences: false)
        guard fields.count == 9, fields[0] == "AIRGREP/1", fields[1] == "P4 Nano",
              fields[2].count == 12, fields[2].allSatisfy(\.isHexDigit) else { return nil }
        func range(_ at: Int) -> Range<Int>? {
            guard let start = Int(fields[at]), let size = Int(fields[at + 1]), start >= 0,
                  size > 0, start <= Int.max - size else { return nil }
            return start..<start + size
        }
        guard let factory = range(5), let tzdata = range(7) else { return nil }
        self.factory = factory
        self.tzdata = tzdata
        mac = String(fields[2])
        version = String(fields[3])
        variant = String(fields[4])
    }

    static func read(_ port: SerialPort) throws -> Self {
        port.discardInput()
        var nextRequest = ContinuousClock.now
        let deadline = ContinuousClock.now + .seconds(3)
        var line: [UInt8] = []
        while ContinuousClock.now < deadline {
            if ContinuousClock.now >= nextRequest {
                try port.write(Array("\rhello\r".utf8))
                nextRequest = ContinuousClock.now + .milliseconds(600)
            }
            for byte in try port.read(atMost: 256, within: .milliseconds(200)) {
                if byte == 10 || byte == 13 {
                    if let result = Self(String(decoding: line, as: UTF8.self)) { return result }
                    line.removeAll(keepingCapacity: true)
                } else if line.count < 512 { line.append(byte) }
            }
        }
        throw Failure("No AIRGREP P4 Nano HELLO on this UART bridge.")
    }
}

private func resetBridge(_ port: SerialPort, bootloader: Bool) async throws {
    try port.setModemLines(dtr: false, rts: true)
    try await Task.sleep(for: .milliseconds(100))
    try port.setModemLines(dtr: bootloader, rts: false)
    try await Task.sleep(for: .milliseconds(100))
    try port.setModemLines(dtr: false, rts: false)
}

struct BridgeMaintenanceView: View {
    let device: Device
    @State private var identity: BridgeIdentity?
    @State private var bootloaderPort: SerialPort?
    @State private var busy = false
    @State private var message = ""
    @State private var choosing = false
    @State private var image: Maintenance.Image?

    var body: some View {
        VStack(spacing: 12) {
            Text(identity.map { "P4 Nano · \($0.mac)" } ?? device.name).font(.title2)
            if let identity {
                Text("\(identity.variant) · \(identity.version)")
            }
            HStack {
                Button("Device Info") { run { _, _ in "Connected" } }.disabled(bootloaderPort != nil)
                if identity != nil {
                    Button("Reboot") { run { port, _ in try await resetBridge(port, bootloader: false); return "Restarted" } }
                    Button("Bootloader") {
                        run(keepPort: true) { port, _ in
                            try await resetBridge(port, bootloader: true)
                            try FlashUtil(port, clock: ContinuousClock()).connect(p4: true)
                            return "In bootloader"
                        }
                    }.disabled(bootloaderPort != nil)
                    Button("Install…") { choosing = true }
                }
            }
            Text(message)
        }
        .padding().disabled(busy)
        .fileImporter(isPresented: $choosing, allowedContentTypes: [.data]) { result in
            do { image = try Maintenance.Image(result.get(), board: "P4 Nano") }
            catch { message = error.localizedDescription }
        }
        .alert("Install \(image?.file.lastPathComponent ?? "")?", isPresented: Binding(
            get: { image != nil }, set: { if !$0 { image = nil } }
        ), presenting: image) { selected in
            Button("Install") {
                run { port, hello in
                    guard selected.partition == (selected.kind == .firmware ? hello.factory : hello.tzdata) else {
                        throw Failure("Installed partition differs from the board layout.")
                    }
                    try await resetBridge(port, bootloader: true)
                    let flash = FlashUtil(port, clock: ContinuousClock())
                    try flash.connect(p4: true)
                    try flash.write(selected.bytes, in: selected.partition, flashSize: selected.flashSize) { _ in }
                    try await resetBridge(port, bootloader: false)
                    return "Verified and restarted"
                }
            }
            Button("Cancel", role: .cancel) {}
        } message: { Text($0.summary) }
    }

    private func run(keepPort: Bool = false, _ action: @escaping @Sendable (SerialPort, BridgeIdentity) async throws -> String) {
        busy = true
        message = "Working…"
        let path = device.console, expected = identity, retained = bootloaderPort
        Task {
            defer { busy = false }
            do {
                let (hello, port, result) = try await Task.detached {
                    let port = try retained ?? SerialPort(path: path)
                    let hello: BridgeIdentity
                    if retained != nil, let expected { hello = expected } else { hello = try BridgeIdentity.read(port) }
                    guard expected == nil || expected?.mac == hello.mac else { throw Failure("Device changed.") }
                    return (hello, port, try await action(port, hello))
                }.value
                identity = hello
                bootloaderPort = keepPort ? port : nil
                message = result
            } catch {
                bootloaderPort = nil
                identity = nil
                message = error.localizedDescription
            }
        }
    }
}

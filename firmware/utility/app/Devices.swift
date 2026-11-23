import SwiftUI

/// A device this computer can talk to. Its serial number is what it is.
struct Device: Identifiable, Hashable {
    let serial: String
    let hasDataInterface: Bool
    /// The serial port of its console, or empty. The demo device has a console without a port.
    let console: String

    var id: String { serial }
    var isEmulated: Bool { serial == AIRGREP_EMULATED }
    var name: String { isEmulated ? "Demo Device" : "airgrep-\(serial)" }
    var hasConsole: Bool { isEmulated || !console.isEmpty }

    /// An emulated device in flight, which is always there.
    static let demo = Device(serial: AIRGREP_EMULATED, hasDataInterface: true, console: "")

    /// What is on the USB bus now, and why it could not be asked when that is so.
    static func connected() -> (devices: [Device], problem: String) {
        var buffer = [airgrep_device](repeating: airgrep_device(), count: 16)
        var problem = [CChar](repeating: 0, count: 256)
        let count = airgrep_devices(&buffer, buffer.count, &problem)
        let devices = buffer.prefix(count)
            .map { found -> Device in
                let serial = text(found.serial)
                let console = SerialPort.path(vendor: AIRGREP_USB_VENDOR, product: AIRGREP_USB_PRODUCT, serial: serial)
                return Device(serial: serial, hasDataInterface: found.data, console: console ?? "")
            }
            .sorted { $0.serial < $1.serial }
        return (devices, text(problem))
    }
}

/// The text in a buffer a C function wrote, up to its end.
func text(_ characters: [CChar]) -> String {
    String(decoding: characters.prefix { $0 != 0 }.map { UInt8(bitPattern: $0) }, as: UTF8.self)
}

/// The text in a C character array, up to its end or the end of the field.
func text<Field>(_ field: Field) -> String {
    withUnsafeBytes(of: field) { String(decoding: $0.prefix { $0 != 0 }, as: UTF8.self) }
}

struct DevicesView: View {
    @EnvironmentObject private var utility: Utility

    var body: some View {
        NavigationSplitView {
            List(selection: $utility.selected) {
                Section("Devices") {
                    ForEach(utility.devices) { row($0) }
                    if !utility.listingProblem.isEmpty {
                        Label(utility.listingProblem, systemImage: "exclamationmark.triangle")
                            .font(.callout)
                            .foregroundStyle(.secondary)
                            .selectionDisabled()
                    } else if utility.devices.isEmpty {
                        Text("Connect an airgrep with a USB cable.")
                            .font(.callout)
                            .foregroundStyle(.secondary)
                            .selectionDisabled()
                    }
                }
                Section("Without a Device") {
                    row(.demo)
                }
            }
            // A double click or Return connects, as it opens a file in a list of files.
            .contextMenu(forSelectionType: Device.ID.self) { _ in } primaryAction: { chosen in
                if let device = utility.listed.first(where: { chosen.contains($0.id) }),
                   device.hasDataInterface, utility.session?.device.id != device.id {
                    utility.connect(device)
                }
            }
            .navigationSplitViewColumnWidth(min: 220, ideal: 240)
            .alert(utility.refusal?.title ?? "", isPresented: Binding(get: { utility.refusal != nil }, set: { if !$0 { utility.refusal = nil } })) {
                Button("OK") {}
            } message: {
                Text(utility.refusal?.text ?? "")
            }
        } detail: {
            if let device = utility.selectedDevice {
                DeviceView(device: device)
            } else {
                ContentUnavailableView(
                    "No Device", systemImage: "cable.connector.slash",
                    description: Text("Connect an airgrep with a USB cable, or try the demo device."))
            }
        }
    }

    private func row(_ device: Device) -> some View {
        Label {
            VStack(alignment: .leading, spacing: 2) {
                Text(device.name)
                if let session = utility.session, session.device.id == device.id {
                    SessionCaption(session: session)
                } else if !device.hasDataInterface {
                    Text("Firmware too old").font(.caption).foregroundStyle(.secondary)
                }
            }
        } icon: {
            DeviceIcon(device: device, session: utility.session.flatMap { $0.device.id == device.id ? $0 : nil })
        }
        .padding(.vertical, 2)
        .tag(device.id)
    }
}

/// The symbol of a device in the list, with a green dot while the device is connected.
private struct DeviceIcon: View {
    let device: Device
    let session: Session?

    /// The dot and the gap that sets it off from the symbol. The tuning knobs.
    private static let dot = 7.0, gap = 1.5, side = 20.0

    var body: some View {
        if let session {
            Badged(icon: icon, session: session)
        } else {
            icon
        }
    }

    @ViewBuilder private var icon: some View {
        if device.isEmulated {
            Image(systemName: "airplane.circle")
        } else {
            Image(nsImage: Artwork.device).resizable().frame(width: Self.side, height: Self.side)
        }
    }

    private struct Badged<Icon: View>: View {
        let icon: Icon
        @ObservedObject var session: Session

        var body: some View {
            if session.details.phase == .connected {
                let hole = DeviceIcon.dot + 2 * DeviceIcon.gap
                icon
                    .mask {
                        // A mask of a fixed colour: the foreground fades when the window does.
                        Rectangle().fill(.black).overlay(alignment: .bottomTrailing) {
                            Circle().fill(.black).frame(width: hole, height: hole).offset(x: hole / 3, y: hole / 3)
                                .blendMode(.destinationOut)
                        }
                        .compositingGroup()
                    }
                    .overlay(alignment: .bottomTrailing) {
                        Circle().fill(.green).frame(width: DeviceIcon.dot, height: DeviceIcon.dot)
                            .offset(x: hole / 3 - DeviceIcon.gap, y: hole / 3 - DeviceIcon.gap)
                    }
                    .accessibilityLabel("Connected")
            } else {
                icon
            }
        }
    }
}

/// How the conversation with the device of a row stands.
private struct SessionCaption: View {
    @ObservedObject var session: Session

    var body: some View {
        Text(caption).font(.caption).foregroundStyle(.secondary)
    }

    private var caption: String {
        switch session.details.phase {
        case .connecting: "Connecting"
        case .connected: "Connected"
        case .ended: "Disconnected"
        }
    }
}

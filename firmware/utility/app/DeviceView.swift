import SwiftUI
import UniformTypeIdentifiers

/// One device: what it says about itself and what can be done with it.
struct DeviceView: View {
    @EnvironmentObject private var utility: Utility
    let device: Device

    private static let logoHeight = 40.0

    var body: some View {
        if device.bridge {
            BridgeMaintenanceView(device: device).id(device.id)
        } else if let session = utility.session, session.device.id == device.id {
            ConnectedView(device: device, session: session)
                .id(ObjectIdentifier(session))
        } else {
            VStack(spacing: 16) {
                if device.isEmulated {
                    Image(systemName: "airplane.circle")
                        .font(.system(size: 48))
                        .foregroundStyle(.secondary)
                } else {
                    // The mark stands where the dot grid of the logo does.
                    HStack(spacing: 6) {
                        Image(nsImage: Artwork.device).resizable().frame(width: Self.logoHeight, height: Self.logoHeight)
                        Image(nsImage: Artwork.logo).resizable().scaledToFit().frame(height: Self.logoHeight)
                    }
                    .foregroundStyle(.secondary)
                    .accessibilityElement()
                    .accessibilityLabel("airgrep")
                }
                Text(device.name).font(.title2)
                if device.isEmulated {
                    Text("An emulated device in flight, to try the utility without hardware.")
                        .foregroundStyle(.secondary)
                } else if !device.hasDataInterface {
                    Text("This firmware has no data interface. Install a newer one with cmake --workflow --preset s3-flash.")
                        .foregroundStyle(.secondary)
                }
                Button("Connect") { utility.connect(device) }
                    .keyboardShortcut(.defaultAction)
                    .controlSize(.large)
                    .disabled(!device.hasDataInterface)
            }
            .multilineTextAlignment(.center)
            .padding(40)
        }
    }
}

extension Usage {
    var color: Color {
        switch self {
        case .low: .green
        case .raised: .yellow
        case .high: .red
        }
    }
}

/// Three steps on top of each other, lit from the bottom up to the usage.
private struct UsageSteps: View {
    let usage: Usage

    var body: some View {
        VStack(spacing: 2) {
            ForEach(Usage.allCases.reversed(), id: \.self) { step in
                RoundedRectangle(cornerRadius: 1.5)
                    .fill(step.rawValue <= usage.rawValue ? AnyShapeStyle(step.color) : AnyShapeStyle(.quaternary))
                    .frame(width: 11, height: 4.5)
            }
        }
        .accessibilityHidden(true)
    }
}

/// How much is used, as a bar in the colour of the usage and as a number.
private struct UsageBar: View {
    let percent: Int

    private static let width = 140.0, height = 6.0

    var body: some View {
        HStack(spacing: 8) {
            Capsule().fill(.quaternary)
                .frame(width: Self.width, height: Self.height)
                .overlay(alignment: .leading) {
                    Capsule().fill(Usage(percent: percent).color)
                        .frame(width: max(Self.height, Self.width * Double(percent) / 100))
                }
            Text("\(percent) %").monospacedDigit().frame(minWidth: 34, alignment: .trailing)
        }
        .accessibilityElement()
        .accessibilityValue("\(percent) percent used")
    }
}

/// What needs a yes before it happens.
private enum Question {
    case bootloader, resetSettings, install(Maintenance.Image)
}

/// What the view asks the user, or has to tell.
private final class Prompt: ObservableObject {
    @Published var asking: Question?
    @Published var problem: String?
    @Published var choosing = false
    @Published var hoveringEnableLink = false
    var kind = Maintenance.Kind.firmware

    func choose(_ kind: Maintenance.Kind) {
        self.kind = kind
        choosing = true
    }
}

private struct ConnectedView: View {
    @EnvironmentObject private var utility: Utility
    @Environment(\.openWindow) private var openWindow
    let device: Device
    @ObservedObject var session: Session
    @StateObject private var prompt = Prompt()

    private var details: Details { session.details }
    private var connected: Bool { details.phase == .connected }

    var body: some View {
        Form {
            Section {
                LabeledContent("Device ID", value: session.device.name)
                LabeledContent("State") { state }
            }
            if details.phase != .connecting, !details.firmware.isEmpty {
                Section("Device") {
                    LabeledContent("Firmware", value: details.firmware)
                    LabeledContent("Model", value: "\(details.board) \(details.chip)")
                    LabeledContent("Mode", value: details.mode.capitalized)
                    LabeledContent("Health") { health }
                    LabeledContent("Uptime", value: uptime)
                    LabeledContent("CPU") { load }
                    LabeledContent("Internal RAM") { memory(details.internalFree, details.internalMinimum, details.internalTotal) }
                    LabeledContent("PSRAM") { memory(details.psramFree, details.psramMinimum, details.psramTotal) }
                }
            }
            Section("Remote Display") {
                if connected && !details.isReadable {
                    LabeledContent {
                        Link(destination: Self.serviceCommands) {
                            Text("How to Enable").underline(prompt.hoveringEnableLink)
                        }
                        .pointerStyle(.link)
                        .onHover { prompt.hoveringEnableLink = $0 }
                    } label: {
                        Text("Enable service commands for Remote Display").foregroundStyle(.secondary)
                    }
                } else {
                    Picker("Display", selection: $utility.variant) {
                        ForEach(DisplayLibrary.variants, id: \.self) { Text($0) }
                    }
                    .disabled(session.watching)
                    .help(session.watching ? "Close the Remote Display to choose another one" : "")
                    Picker("Refresh", selection: $utility.interval) {
                        Text("0.1 s").tag(Duration.milliseconds(100))
                        Text("0.5 s").tag(Duration.milliseconds(500))
                        Text("1 s").tag(Duration.seconds(1))
                    }
                    .pickerStyle(.segmented)
                    Toggle("Click Sound", isOn: $utility.clicks)
                    LabeledContent {
                        Button(session.watching ? "Show Remote Display" : "Open Remote Display") {
                            openWindow(id: RemoteDisplayWindow.id)
                        }
                        .disabled(!connected || DisplayLibrary.variants.isEmpty)
                    } label: {
                        Text(displayNote).foregroundStyle(.secondary)
                    }
                }
            }
            Section("Maintenance") {
                LabeledContent("Console") {
                    Button("Open Console") {
                        utility.openConsole(of: device)
                        openWindow(id: ConsoleWindow.id)
                    }
                    .disabled(!device.hasConsole)
                    .help(device.hasConsole ? "" : "This device shows no console port")
                }
                LabeledContent("Restart") {
                    Button("Restart") { session.reboot() }.disabled(!connected)
                    Button("Enter Bootloader") { prompt.asking = .bootloader }.disabled(!connected)
                }
                LabeledContent("Settings") {
                    Button("Reset") { prompt.asking = .resetSettings }.disabled(!connected || !details.isReadable)
                }
                .help(connected && !details.isReadable ? "This firmware has no service commands. A debug build resets its settings." : "")
                LabeledContent("Install") {
                    ForEach(Maintenance.Kind.allCases, id: \.self) { kind in
                        Button(kind.buttonTitle) { prompt.choose(kind) }
                            .disabled(!installable)
                    }
                }
                .help(session.device.isEmulated ? "The demo device takes no files" : "")
            }
            Section {
                HStack {
                    Button("Disconnect") { utility.disconnect() }
                    Button("Show Data") { openWindow(id: DataWindow.id) }
                        .disabled(!connected || !details.isReadable)
                }
            }
        }
        .formStyle(.grouped)
        .confirmationDialog(
            question, isPresented: Binding(get: { prompt.asking != nil }, set: { if !$0 { prompt.asking = nil } }),
            titleVisibility: .visible,
            presenting: prompt.asking
        ) { asked in
            Button(confirmation(asked)) { carryOut(asked) }
            Button("Cancel", role: .cancel) {}
        } message: { asked in
            Text(consequence(asked))
        }
        .alert("Cannot Install", isPresented: Binding(get: { prompt.problem != nil }, set: { if !$0 { prompt.problem = nil } })) {
            Button("OK") {}
        } message: {
            Text(prompt.problem ?? "")
        }
        .fileImporter(isPresented: $prompt.choosing, allowedContentTypes: [UTType(filenameExtension: "bin") ?? .data]) { chosen in
            do {
                prompt.asking = .install(try Maintenance.Image(try chosen.get(), prompt.kind, board: details.board))
            } catch CocoaError.userCancelled {
            } catch {
                prompt.problem = error.localizedDescription
            }
        }
        .fileDialogMessage(prompt.kind.filePrompt)
        .fileDialogConfirmationLabel("Choose")
        .onAppear { takeOffered() }
        .onChange(of: utility.offered) { takeOffered() }
        .sheet(item: $session.installation) { installation in
            InstallSheet(installation: installation) { session.installation = nil }
        }
    }

    @ViewBuilder private var state: some View {
        switch details.phase {
        case .connecting:
            HStack(spacing: 6) {
                ProgressView().controlSize(.small)
                Text("Connecting")
            }
        case .connected:
            Label("Connected", systemImage: "checkmark.circle.fill").foregroundStyle(.green)
        case .ended:
            // What was asked for and failed, as a reset the device refused, ended with its reason.
            let asked = details.error.isEmpty ? session.asked : nil
            VStack(alignment: .trailing, spacing: 4) {
                switch asked {
                case .restart: Label("Restarting", systemImage: "arrow.clockwise.circle").foregroundStyle(.secondary)
                case .bootloader: Label("In its bootloader", systemImage: "cpu").foregroundStyle(.secondary)
                case nil: Label("Disconnected", systemImage: "xmark.circle").foregroundStyle(.red)
                }
                if asked == nil {
                    Text(details.error).font(.callout).foregroundStyle(.secondary)
                        .textSelection(.enabled)
                }
                Button("Connect Again") { utility.connect(session.device) }
            }
        }
    }

    @ViewBuilder private var health: some View {
        if details.health == 0 {
            Label("System OK", systemImage: "checkmark.circle.fill").foregroundStyle(.green)
        } else {
            Label(details.healthText, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.orange)
        }
    }

    private var uptime: String {
        let seconds = details.uptime
        return String(format: "%llud %02llu:%02llu:%02llu", seconds / 86400, seconds / 3600 % 24, seconds / 60 % 60, seconds % 60)
    }

    @ViewBuilder private var load: some View {
        let known = zip(["core0", "core1", "overall"], details.load).compactMap { name, percent in
            percent.map { (name: name, percent: $0) }
        }
        if known.isEmpty {
            Text("Not measured")
        } else {
            HStack(spacing: 14) {
                ForEach(known, id: \.name) { core in
                    HStack(spacing: 5) {
                        UsageSteps(usage: Usage(percent: core.percent))
                        Text("\(core.name) \(core.percent) %").monospacedDigit()
                    }
                }
            }
        }
    }

    @ViewBuilder private func memory(_ free: UInt32, _ minimum: UInt32, _ total: UInt32) -> some View {
        if total == 0 {
            Text("None")
        } else {
            UsageBar(percent: Usage.percent(free: free, of: total))
                .help("\(free / 1024) KB free, \(minimum / 1024) KB minimum, \(total / 1024) KB total")
        }
    }

    private static let serviceCommands = URL(string: "https://github.com/mp911de/airgrep/tree/main/firmware#service-commands")!

    private var displayNote: String {
        if session.watching && details.pictures > 0 {
            return String(format: "%llu pictures, the last read in %.0f ms", details.pictures, details.readMilliseconds)
        }
        return "The display of the device, in a window."
    }

    /// Installing sends the device into its bootloader, so it has to answer first.
    private var installable: Bool { connected && !session.device.isEmulated }

    /// A file dropped on the application says itself whether it is firmware or a time zone table.
    private func takeOffered() {
        guard let file = utility.offered else { return }
        utility.offered = nil
        guard installable, session.installation == nil else { return }
        do {
            prompt.asking = .install(try Maintenance.Image(file, board: details.board))
        } catch {
            prompt.problem = error.localizedDescription
        }
    }

    private var question: String {
        switch prompt.asking {
        case .bootloader: "Restart \(session.device.name) into its bootloader?"
        case .resetSettings: "Reset the settings of \(session.device.name)?"
        case .install(let image): "Install \(image.file.lastPathComponent) on \(session.device.name)?"
        case nil: ""
        }
    }

    private func confirmation(_ asked: Question) -> String {
        switch asked {
        case .bootloader: "Enter Bootloader"
        case .resetSettings: "Reset"
        case .install: "Install"
        }
    }

    private func consequence(_ asked: Question) -> String {
        switch asked {
        case .bootloader:
            "The device stays in its bootloader until it is flashed or power cycled."
        case .resetSettings:
            "The device removes every setting it stores and restarts with its defaults."
        case .install(let image):
            "\(image.summary)\n\nRestart to bootloader mode and upgrade?"
        }
    }

    private func carryOut(_ asked: Question) {
        switch asked {
        case .bootloader:
            session.enterBootloader()
        case .resetSettings:
            session.resetSettings()
        case .install(let image):
            let installation = Installation(image, on: session.device)
            session.installation = installation
            session.enterBootloader()
            installation.start()
        }
    }
}

import SwiftUI

@main
struct UtilityApp: App {
    @StateObject private var utility = Utility()

    var body: some Scene {
        Window("airgrep Utility", id: "devices") {
            DevicesView()
                .environmentObject(utility)
                // A file dropped on the icon in the Dock arrives here.
                .onOpenURL { utility.offer($0) }
                .onAppear { DockRipple.playOnce() }
        }
        .defaultSize(width: 780, height: 640)
        .commands {
            CommandGroup(replacing: .appInfo) {
                Button("About airgrep Utility") {
                    NSApp.orderFrontStandardAboutPanel(options: [.applicationVersion: "", .version: utilityVersion])
                }
            }
            TerminalCommands(utility: utility)
        }

        Window("Remote Display", id: RemoteDisplayWindow.id) {
            RemoteDisplayWindow()
                .environmentObject(utility)
        }
        .windowResizability(.contentSize)
        // It shows a conversation, and there is none at launch or for the Window menu to open.
        .restorationBehavior(.disabled)
        .defaultLaunchBehavior(.suppressed)
        .commandsRemoved()

        Window("Device Data", id: DataWindow.id) {
            DataWindow()
                .environmentObject(utility)
        }
        .defaultSize(width: 760, height: 720)
        .restorationBehavior(.disabled)
        .defaultLaunchBehavior(.suppressed)
        .commandsRemoved()

        Window("Console", id: ConsoleWindow.id) {
            ConsoleWindow()
                .environmentObject(utility)
        }
        .windowResizability(.contentSize)
        .restorationBehavior(.disabled)
        .defaultLaunchBehavior(.suppressed)
        .commandsRemoved()
    }
}

/// A window whose conversation or console is gone has nothing left to show, so it dismisses itself.
struct EmptyWindow: View {
    let id: String
    @Environment(\.dismissWindow) private var dismissWindow

    var body: some View {
        Color.clear
            .frame(width: 320, height: 200)
            .onAppear { dismissWindow(id: id) }
    }
}

/// View menu: the text of the console and the Device Data window, whichever is in front.
struct TerminalCommands: Commands {
    @ObservedObject var utility: Utility
    @FocusedObject private var console: Console?
    @FocusedObject private var reading: Reading?

    var body: some Commands {
        CommandGroup(before: .toolbar) {
            Button("Bigger") { utility.terminalFont = utility.terminalFont.bigger }
                .keyboardShortcut("+", modifiers: .command)
            Button("Smaller") { utility.terminalFont = utility.terminalFont.smaller }
                .keyboardShortcut("-", modifiers: .command)
            Button("Default Size") { utility.terminalFont = utility.terminalFont.standard }
                .keyboardShortcut("0", modifiers: .command)
            Divider()
            Menu("Font") {
                Toggle("System Monospaced", isOn: Binding(
                    get: { utility.terminalFont.name.isEmpty },
                    set: { _ in utility.terminalFont.name = "" }))
                ForEach(TerminalFont.families, id: \.self) { family in
                    Toggle(family, isOn: Binding(
                        get: { utility.terminalFont.isOf(family: family) },
                        set: { _ in utility.terminalFont = utility.terminalFont.of(family: family) }))
                }
                Divider()
                Button("Other…") { utility.chooseFont() }
            }
            Divider()
            Button("Clear") { console?.clear() }
                .keyboardShortcut("k", modifiers: .command)
                .disabled(console == nil)
            Button("Export…") {
                if let console { console.export() } else { reading?.export() }
            }
            .keyboardShortcut("s", modifiers: .command)
            .disabled(console == nil && reading == nil)
            Divider()
        }
    }
}

/// The font panel of the system, which tells what the user chose in it.
@MainActor
final class FontChooser: NSObject {
    /// A font of the panel may have any size. These are the ones that are legible in a console.
    static let sizes = 8.0...36.0

    private var current = NSFont.monospacedSystemFont(ofSize: 12, weight: .regular)
    private var chosen: (NSFont) -> Void = { _ in }

    func show(_ font: NSFont, chosen: @escaping (NSFont) -> Void) {
        current = font
        self.chosen = chosen
        let manager = NSFontManager.shared
        manager.target = self
        manager.action = #selector(changeFont(_:))
        manager.setSelectedFont(font, isMultiple: false)
        manager.orderFrontFontPanel(nil)
    }

    @objc func changeFont(_ sender: NSFontManager?) {
        guard let sender else { return }
        let font = sender.convert(current)
        current = NSFont(descriptor: font.fontDescriptor, size: min(max(font.pointSize, Self.sizes.lowerBound), Self.sizes.upperBound)) ?? font
        chosen(current)
    }
}

/// The devices on this computer and the one the user talks to.
@MainActor
final class Utility: ObservableObject {
    /// What is on the USB bus now.
    @Published private(set) var devices: [Device] = []
    /// Why the USB bus could not be asked, or empty.
    @Published private(set) var listingProblem = ""
    @Published var selected: Device.ID?
    /// The one conversation. A device is talked to after Connect only.
    @Published private(set) var session: Session?
    /// The display variant a Remote Display shows, remembered between launches.
    @Published var variant: String {
        didSet { UserDefaults.standard.set(variant, forKey: "variant") }
    }
    /// The time from one picture of the device to the next.
    @Published var interval = Duration.milliseconds(500) {
        didSet { session?.rewatch(every: interval) }
    }
    /// Whether a press on a Remote Display clicks as the buzzer of the device does.
    @Published var clicks: Bool {
        didSet { UserDefaults.standard.set(clicks, forKey: "clicks") }
    }
    /// The font of the console and the Device Data window, remembered between launches.
    @Published var terminalFont: TerminalFont {
        didSet {
            UserDefaults.standard.set(terminalFont.name, forKey: "terminalFont")
            UserDefaults.standard.set(Double(terminalFont.size), forKey: "terminalFontSize")
        }
    }
    /// A file the user gave the application, until the page of the connected device has taken it.
    @Published var offered: URL?
    /// Why a file the user gave could not be taken, as title and text.
    @Published var refusal: (title: String, text: String)?
    /// The console that is open, one at a time.
    @Published private(set) var console: Console?
    /// The library of the chosen variant, once a Remote Display asked for it.
    @Published private(set) var display: Result<DisplayLibrary, DisplayLibrary.Failure>?

    private var listing = false
    private var timer: Timer?
    /// Whether the device of the conversation is still on the bus.
    private var presence = Presence()

    init() {
        let stored = UserDefaults.standard.string(forKey: "variant")
        variant = stored.flatMap { DisplayLibrary.variants.contains($0) ? $0 : nil }
            ?? DisplayLibrary.variants.first ?? ""
        clicks = UserDefaults.standard.object(forKey: "clicks") as? Bool ?? true
        let size = UserDefaults.standard.double(forKey: "terminalFontSize")
        terminalFont = TerminalFont(
            name: UserDefaults.standard.string(forKey: "terminalFont") ?? "",
            size: FontChooser.sizes.contains(size) ? CGFloat(size) : TerminalFont.standardSize)
        list()
        timer = Timer.scheduledTimer(withTimeInterval: 1, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.list() }
        }
    }

    /// The device the detail shows. The one of the conversation stays while a listing misses it,
    /// until `presence` says the conversation is over.
    var selectedDevice: Device? {
        listed.first { $0.id == selected }
            ?? session.flatMap { $0.device.id == selected ? $0.device : nil }
    }

    /// The devices of the list: the demo device, then what is on the bus.
    var listed: [Device] { [Device.demo] + devices }

    func connect(_ device: Device) {
        guard device.hasDataInterface else { return }
        disconnect()
        session = Session(device)
    }

    func disconnect() {
        session?.close()
        session = nil
        presence = Presence()
    }

    /// A file can be installed on a device that answers. The page of that device checks it and asks.
    func offer(_ file: URL) {
        let name = file.lastPathComponent
        guard Maintenance.Image.kind(of: file) != nil else {
            refusal = ("Cannot Install", "\(name) is neither airgrep firmware nor a time zone table.")
            return
        }
        guard let session, session.details.phase == .connected else {
            refusal = ("Connect an airgrep First", "\(name) is installed on a connected device.")
            return
        }
        guard !session.device.isEmulated else {
            refusal = ("Cannot Install", "The demo device takes no files.")
            return
        }
        guard session.installation == nil else {
            refusal = ("Cannot Install", "An installation is already in progress.")
            return
        }
        selected = session.device.id
        offered = file
    }

    private let fontChooser = FontChooser()

    /// Shows the font panel of the system, for a font the View menu does not list.
    func chooseFont() {
        fontChooser.show(terminalFont.font) { [weak self] in
            self?.terminalFont = TerminalFont(name: $0.fontName, size: $0.pointSize)
        }
    }

    func loadDisplay() {
        display = DisplayLibrary.load(variant)
    }

    /// The console stays when its window closes, with its port closed and its text kept. A window
    /// that lost its content would dismiss itself the next time it is shown.
    func openConsole(of device: Device) {
        if console?.device != device {
            console?.close()
            console = Console(device)
        } else if console?.isOpen == false {
            console?.open()
        }
    }

    /// Listing asks the USB bus, which may take a moment, so it runs beside the user interface.
    private func list() {
        guard !listing else { return }
        listing = true
        Task.detached(priority: .utility) {
            let (found, problem) = Device.connected()
            await MainActor.run {
                self.listing = false
                if found != self.devices {
                    self.devices = found
                }
                if problem != self.listingProblem {
                    self.listingProblem = problem
                }
                // The demo device is on no bus. A bus that could not be asked says nothing of a device.
                if let session = self.session, !session.device.isEmulated, problem.isEmpty,
                   self.presence.isOver(
                       ended: session.details.phase == .ended, asked: session.asked != nil,
                       listed: found.contains { $0.id == session.device.id }, held: session.isHeld) {
                    self.disconnect()
                }
                if self.selectedDevice == nil, let first = found.first {
                    self.selected = first.id
                }
            }
        }
    }
}

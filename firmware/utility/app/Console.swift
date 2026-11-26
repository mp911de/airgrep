import SwiftUI

/// The console of a device: what it prints, and the keys it gets. The firmware edits the line and
/// keeps the history, as it does for any terminal.
@MainActor
final class Console: ObservableObject {
    let device: Device
    var columns = TerminalText.defaultColumns
    @Published private(set) var terminal = TerminalText()
    /// Why the console does not run, or nil while it does.
    @Published private(set) var ended: String?

    private var port: (any SerialLink & Sendable)?
    /// Serializes writes off the main thread so a stalled port cannot freeze the UI.
    private let writeQueue = DispatchQueue(label: "airgrep.console.write", qos: .userInitiated)
    /// Signals when the previous reader thread has exited and dropped its port reference.
    private var readerDone: DispatchGroup?
    /// Counts the ports opened, so a reader of an earlier one stops.
    private var generation = 0
    private var opening = false

    /// Control+L. The line editor of the firmware clears the screen and writes its prompt again, so
    /// a console that opens shows a prompt and runs nothing.
    private static let redraw: [UInt8] = [0x0C]

    init(_ device: Device) {
        self.device = device
        open()
    }

    func open() {
        close()
        ended = nil
        opening = true
        let generation = generation
        let previousReader = readerDone
        let done = DispatchGroup()
        done.enter()
        readerDone = done
        let device = device
        let writeQueue = writeQueue
        Thread.detachNewThread { [weak self] in
            defer { done.leave() }
            // Wait for any prior reader and queued write to finish releasing the old fd before reopening.
            previousReader?.wait()
            writeQueue.sync {}
            var failure: String?
            // Inner scope ensures `opened` deallocates and closes its fd before `done.leave()`.
            do {
                let opened: any SerialLink & Sendable
                if device.isEmulated {
                    opened = EmulatedConsole()
                } else {
                    let path = SerialPort.path(
                        vendor: Int(AIRGREP_USB_VENDOR), product: Int(AIRGREP_USB_PRODUCT), serial: device.serial
                    ) ?? device.console
                    guard !path.isEmpty else {
                        throw Failure("This device shows no console port.")
                    }
                    opened = try SerialPort(path: path)
                }
                let accepted = DispatchQueue.main.sync {
                    MainActor.assumeIsolated { self?.attach(opened, for: generation) ?? false }
                }
                guard accepted else { return }
                while true {
                    let bytes = try opened.read(atMost: 4096, within: .milliseconds(200))
                    let running = DispatchQueue.main.sync {
                        MainActor.assumeIsolated { self?.received(bytes, from: generation) ?? false }
                    }
                    if !running { return }
                }
            } catch is CancellationError {
                return
            } catch {
                failure = error.localizedDescription
            }
            if let failure {
                DispatchQueue.main.async {
                    MainActor.assumeIsolated { self?.end(failure, of: generation) }
                }
            }
        }
    }

    var isOpen: Bool { opening || port != nil }

    func close() {
        generation += 1
        opening = false
        port?.cancel()
        port = nil
    }

    func send(_ bytes: [UInt8]) {
        guard let port, !bytes.isEmpty else { return }
        let generation = generation
        writeQueue.async { [weak self] in
            do {
                try port.write(bytes)
            } catch is CancellationError {
            } catch {
                let message = error.localizedDescription
                DispatchQueue.main.async {
                    MainActor.assumeIsolated { self?.end(message, of: generation) }
                }
            }
        }
    }

    private func attach(_ opened: any SerialLink & Sendable, for generation: Int) -> Bool {
        guard generation == self.generation else { return false }
        opening = false
        port = opened
        send(Self.redraw)
        return true
    }

    /// Forgets what was printed and has the line editor write its prompt again.
    func clear() {
        terminal.clear()
        send(Self.redraw)
    }

    func export() {
        Export.save(terminal.text, as: Export.name(of: device.name, "console"))
    }

    /// False once the port these bytes came from is not the one of the console any more.
    private func received(_ bytes: [UInt8], from generation: Int) -> Bool {
        guard generation == self.generation else { return false }
        if !bytes.isEmpty {
            send(terminal.feed(bytes, columns: columns))
        }
        return true
    }

    private func end(_ reason: String, of generation: Int) {
        guard generation == self.generation else { return }
        close()
        ended = reason
    }
}

struct ConsoleWindow: View {
    static let id = "console"

    @EnvironmentObject private var utility: Utility

    var body: some View {
        if let console = utility.console {
            Shown(console: console)
                .id(ObjectIdentifier(console))
        } else {
            EmptyWindow(id: Self.id)
        }
    }

    private struct Shown: View {
        @EnvironmentObject private var utility: Utility
        @ObservedObject var console: Console

        var body: some View {
            // The size a console opens with. A font chosen later resizes the window itself, see `apply`.
            let opening = TerminalLook(TerminalFont())
            TerminalView(console: console, look: TerminalLook(utility.terminalFont))
                .frame(minWidth: 320, idealWidth: opening.width(columns: TerminalText.defaultColumns), maxWidth: .infinity,
                       minHeight: 240, idealHeight: opening.height(rows: TerminalLook.initialRows), maxHeight: .infinity)
                .focusedSceneObject(console)
                .toolbar {
                    Button { utility.terminalFont = utility.terminalFont.smaller } label: {
                        Label("Smaller", systemImage: "textformat.size.smaller")
                    }
                    .help("Makes the text smaller")
                    Button { utility.terminalFont = utility.terminalFont.bigger } label: {
                        Label("Bigger", systemImage: "textformat.size.larger")
                    }
                    .help("Makes the text bigger")
                    Button { console.clear() } label: { Label("Clear", systemImage: "trash") }
                        .help("Removes everything printed so far")
                    Button { console.export() } label: { Label("Export", systemImage: "square.and.arrow.down") }
                        .help("Saves the text to a file")
                }
                .safeAreaInset(edge: .bottom, spacing: 0) {
                    if let ended = console.ended {
                        HStack {
                            Label(ended, systemImage: "cable.connector.slash")
                            Spacer()
                            Button("Open Again") { console.open() }
                        }
                        .padding(10)
                        .background(.bar)
                    }
                }
                .navigationTitle("Console of \(console.device.name)")
                // The title bar and the bar at the bottom go with the terminal.
                .preferredColorScheme(.dark)
                .onDisappear { console.close() }
        }
    }
}

/// How a console looks: a dark terminal, whatever the appearance of the system. The tuning knobs.
@MainActor
struct TerminalLook {
    static let background = NSColor(white: 0.11, alpha: 1), text = NSColor(white: 0.88, alpha: 1)
    /// The block over the character under the cursor, which still shows through it.
    static let cursor = NSColor(white: 0.88, alpha: 0.55)
    static let inset = NSSize(width: 10, height: 8)
    static let initialRows = 34

    let font: NSFont
    /// The room of one character. A window grows and shrinks by whole ones.
    let cell: NSSize

    init(_ chosen: TerminalFont) {
        font = chosen.font
        cell = NSSize(width: ("0" as NSString).size(withAttributes: [.font: font]).width,
                      height: NSLayoutManager().defaultLineHeight(for: font))
    }

    var attributes: [NSAttributedString.Key: Any] { [.font: font, .foregroundColor: Self.text] }

    func width(columns: Int) -> CGFloat { (CGFloat(columns) * cell.width).rounded(.up) + 2 * Self.inset.width }
    func height(rows: Int) -> CGFloat { (CGFloat(rows) * cell.height).rounded(.up) + 2 * Self.inset.height }
}

/// The font of the console and the Device Data window, which the user chooses.
struct TerminalFont: Equatable {
    /// The sizes Bigger and Smaller step through, and the one a console starts with. The tuning knobs.
    static let sizes: [CGFloat] = [10, 11, 12, 13, 14, 16, 18, 20, 24], standardSize: CGFloat = 12

    /// The PostScript name, or empty for the fixed-width font of the system.
    var name = ""
    var size = TerminalFont.standardSize

    @MainActor var font: NSFont {
        (name.isEmpty ? nil : NSFont(name: name, size: size)) ?? .monospacedSystemFont(ofSize: size, weight: .regular)
    }

    var bigger: TerminalFont { TerminalFont(name: name, size: Self.sizes.first { $0 > size } ?? size) }
    var smaller: TerminalFont { TerminalFont(name: name, size: Self.sizes.last { $0 < size } ?? size) }
    var standard: TerminalFont { TerminalFont(name: name, size: Self.standardSize) }

    /// The installed families of fixed width, by name.
    @MainActor static let families: [String] = {
        let names = NSFontManager.shared.availableFontNames(with: .fixedPitchFontMask) ?? []
        return Set(names.compactMap { NSFont(name: $0, size: standardSize)?.familyName }).sorted()
    }()

    @MainActor func isOf(family: String) -> Bool { !name.isEmpty && font.familyName == family }

    @MainActor func of(family: String) -> TerminalFont {
        let font = NSFontManager.shared.font(withFamily: family, traits: [], weight: 5, size: size)
        return TerminalFont(name: font?.fontName ?? "", size: size)
    }
}

/// A scroll view that stays at the end of its text, where the cursor is, also when it is resized.
/// Only the user's own scrolling stops it: scrolling up to read stops it, scrolling to the end or typing resumes it.
final class FollowingScrollView: NSScrollView {
    private var follows = true
    private var size = NSSize.zero
    private var changes: [NSObjectProtocol] = []

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        changes.forEach(NotificationCenter.default.removeObserver)
        changes = []
        guard window != nil, let documentView else { return }
        contentView.postsBoundsChangedNotifications = true
        documentView.postsFrameChangedNotifications = true
        changes = [
            // Posted for every scroll the user makes, and for no scroll of the program.
            NotificationCenter.default.addObserver(forName: NSScrollView.didLiveScrollNotification, object: self, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.userScrolled() }
            },
            NotificationCenter.default.addObserver(forName: NSView.boundsDidChangeNotification, object: contentView, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.clipChanged() }
            },
            NotificationCenter.default.addObserver(forName: NSView.frameDidChangeNotification, object: documentView, queue: .main) { [weak self] _ in
                MainActor.assumeIsolated { self?.follow() }
            },
        ]
    }

    /// Scrolls to the end unless the user scrolled up or is selecting text.
    func follow() {
        if follows, let text = documentView as? ConsoleTextView, !text.trackingSelection, text.selectedRange().length == 0 {
            documentView?.scrollToEndOfDocument(nil)
        }
    }

    /// Typing is about the cursor: the view goes back to it and follows again.
    func resume() {
        follows = true
        documentView?.scrollToEndOfDocument(nil)
    }

    private func userScrolled() {
        guard let documentView else { return }
        follows = contentView.bounds.maxY >= documentView.frame.maxY - 1
    }

    private func clipChanged() {
        if contentView.bounds.size != size {
            size = contentView.bounds.size
            follow()
        }
    }
}

/// The text of a console. Keys go to the device, which echoes what it makes of them.
private struct TerminalView: NSViewRepresentable {
    @ObservedObject var console: Console
    let look: TerminalLook

    func makeNSView(context: Context) -> FollowingScrollView {
        let scroll = FollowingScrollView()
        scroll.documentView = KeysView(console, look)
        scroll.hasVerticalScroller = true
        // A scroller that takes room of its own would cut the last columns.
        scroll.scrollerStyle = .overlay
        scroll.backgroundColor = TerminalLook.background
        return scroll
    }

    func updateNSView(_ scroll: FollowingScrollView, context: Context) {
        guard let view = scroll.documentView as? KeysView else { return }
        view.apply(look)
        view.show(console.terminal)
        scroll.follow()
    }

    private final class KeysView: ConsoleTextView {
        private let console: Console
        private var look: TerminalLook
        /// What of the terminal is in the view: so many of its lines, which end at this place of the text.
        private var shownLines = 0, lineStart = 0, drops = 0
        /// The place of the cursor in the text.
        private var cursor = 0
        private var keyChanges: [NSObjectProtocol] = []

        init(_ console: Console, _ look: TerminalLook) {
            self.console = console
            self.look = look
            let width = look.width(columns: TerminalText.defaultColumns)
            let container = NSTextContainer(size: NSSize(width: width, height: .greatestFiniteMagnitude))
            container.widthTracksTextView = true
            container.lineFragmentPadding = 0
            container.lineBreakMode = .byCharWrapping
            let layout = NSLayoutManager()
            layout.addTextContainer(container)
            let storage = NSTextStorage()
            storage.addLayoutManager(layout)
            super.init(frame: NSRect(x: 0, y: 0, width: width, height: 0), textContainer: container)
            isEditable = false
            isRichText = false
            // The font is chosen in the View menu, for every console.
            usesFontPanel = false
            textContainerInset = TerminalLook.inset
            backgroundColor = TerminalLook.background
            isVerticallyResizable = true
            isHorizontallyResizable = false
            autoresizingMask = [.width]
            minSize = .zero
            maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: .greatestFiniteMagnitude)
            setAccessibilityLabel("Console of \(console.device.name)")
        }

        @available(*, unavailable)
        required init?(coder: NSCoder) { fatalError("not in a nib") }

        override func setFrameSize(_ newSize: NSSize) {
            super.setFrameSize(newSize)
            countColumns()
        }

        private func countColumns() {
            if let textContainer, textContainer.size.width > 0 {
                console.columns = max(1, Int(textContainer.size.width / look.cell.width))
            }
        }

        func apply(_ look: TerminalLook) {
            guard look.font != self.look.font else { return }
            let old = self.look.cell
            self.look = look
            if let storage = textStorage {
                storage.setAttributes(look.attributes, range: NSRange(location: 0, length: storage.length))
            }
            if let window, let shown = enclosingScrollView?.contentSize {
                // The window keeps its columns and rows, and its top left corner, as far as the screen has room.
                let columns = ((shown.width - 2 * TerminalLook.inset.width) / old.width).rounded()
                let rows = ((shown.height - 2 * TerminalLook.inset.height) / old.height).rounded()
                var frame = window.frame
                let room = window.screen?.visibleFrame ?? frame
                frame.size.width = min(frame.width + columns * (look.cell.width - old.width), room.width)
                frame.size.height = min(frame.height + rows * (look.cell.height - old.height), room.height)
                frame.origin.y = max(window.frame.maxY - frame.height, room.minY)
                frame.origin.x = min(frame.minX, max(room.maxX - frame.width, room.minX))
                window.contentResizeIncrements = look.cell
                window.setFrame(frame, display: true)
            }
            countColumns()
            needsDisplay = true
        }

        /// Brings the text up to the terminal: new lines are added, the line under the cursor is written again.
        func show(_ terminal: TerminalText) {
            guard let storage = textStorage else { return }
            if terminal.drops != drops {
                drops = terminal.drops
                shownLines = 0
                lineStart = 0
            }
            let added = terminal.lines[min(shownLines, terminal.lines.count)...].map { $0 + "\n" }.joined()
            // The cursor may stand behind the end of its line, and needs a place there too.
            let padding = max(terminal.column + 1 - terminal.line.count, 0)
            let line = terminal.line + [Character](repeating: " ", count: padding)
            storage.replaceCharacters(
                in: NSRange(location: lineStart, length: storage.length - lineStart),
                with: NSAttributedString(string: added + String(line), attributes: look.attributes))
            shownLines = terminal.lines.count
            lineStart += added.utf16.count
            cursor = lineStart + String(line.prefix(terminal.column)).utf16.count
            needsDisplay = true
        }

        override func draw(_ dirtyRect: NSRect) {
            super.draw(dirtyRect)
            guard let layoutManager, let textContainer, cursor < (textStorage?.length ?? 0) else { return }
            let glyphs = layoutManager.glyphRange(forCharacterRange: NSRange(location: cursor, length: 1), actualCharacterRange: nil)
            var block = layoutManager.boundingRect(forGlyphRange: glyphs, in: textContainer)
            block.origin.x += textContainerOrigin.x
            block.origin.y += textContainerOrigin.y
            block.size.width = look.cell.width
            TerminalLook.cursor.set()
            // Solid while the keys go here, an outline while they go elsewhere.
            if window?.isKeyWindow == true, window?.firstResponder === self {
                block.fill()
            } else {
                block.insetBy(dx: 0.5, dy: 0.5).frame()
            }
        }

        override func viewDidMoveToWindow() {
            keyChanges.forEach(NotificationCenter.default.removeObserver)
            keyChanges = []
            guard let window else { return }
            window.contentResizeIncrements = look.cell
            // A console is opened to type into.
            window.makeFirstResponder(self)
            keyChanges = [NSWindow.didBecomeKeyNotification, NSWindow.didResignKeyNotification].map {
                NotificationCenter.default.addObserver(forName: $0, object: window, queue: .main) { [weak self] _ in
                    MainActor.assumeIsolated {
                        guard let self, let window = self.window else { return }
                        // A window that is shown again has given the keyboard to nothing.
                        if window.firstResponder === window {
                            window.makeFirstResponder(self)
                        }
                        self.needsDisplay = true
                    }
                }
            }
        }

        override func becomeFirstResponder() -> Bool {
            needsDisplay = true
            return super.becomeFirstResponder()
        }

        override func resignFirstResponder() -> Bool {
            needsDisplay = true
            return super.resignFirstResponder()
        }

        override func keyDown(with event: NSEvent) {
            guard !event.modifierFlags.contains(.command), let characters = event.characters else {
                return super.keyDown(with: event)
            }
            (enclosingScrollView as? FollowingScrollView)?.resume()
            console.send(characters.unicodeScalars.flatMap { scalar -> [UInt8] in
                switch Int(scalar.value) {
                case NSUpArrowFunctionKey: Array("\u{1B}[A".utf8)
                case NSDownArrowFunctionKey: Array("\u{1B}[B".utf8)
                case NSRightArrowFunctionKey: Array("\u{1B}[C".utf8)
                case NSLeftArrowFunctionKey: Array("\u{1B}[D".utf8)
                case NSHomeFunctionKey: Array("\u{1B}[H".utf8)
                case NSEndFunctionKey: Array("\u{1B}[F".utf8)
                case NSDeleteFunctionKey: Array("\u{1B}[3~".utf8)
                // Other keys without a character of their own, such as F1.
                case 0xF700...0xF8FF: []
                default: Array(String(scalar).utf8)
                }
            })
        }

        override func paste(_ sender: Any?) {
            if let text = NSPasteboard.general.string(forType: .string) {
                (enclosingScrollView as? FollowingScrollView)?.resume()
                let normalized = text
                    .replacingOccurrences(of: "\r\n", with: "\r")
                    .replacingOccurrences(of: "\n", with: "\r")
                console.send(Array(normalized.utf8))
            }
        }

        override func validateUserInterfaceItem(_ item: NSValidatedUserInterfaceItem) -> Bool {
            item.action == #selector(paste(_:)) || super.validateUserInterfaceItem(item)
        }
    }
}

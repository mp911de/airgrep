import SwiftUI

/// The data of the device as text, read again with every picture.
struct DataWindow: View {
    static let id = "device-data"

    @EnvironmentObject private var utility: Utility

    var body: some View {
        if let session = utility.session {
            Shown(session: session)
                .id(ObjectIdentifier(session))
        } else {
            EmptyWindow(id: Self.id)
        }
    }
}

/// What the window shows. An object, as the command line tools build no `@State`.
final class Reading: ObservableObject {
    @Published var text = ""
    /// A text that changes under the pointer cannot be selected, so the user holds it.
    @Published var paused = false
    /// The device the text is of, for the name of an export.
    var device = ""

    @MainActor func export() {
        Export.save(text, as: Export.name(of: device, "data"))
    }
}

/// The text of a session.
private struct Shown: View {
    @EnvironmentObject private var utility: Utility
    @ObservedObject var session: Session
    @StateObject private var reading = Reading()

    var body: some View {
        SelectableText(text: reading.text, font: utility.terminalFont.font)
            .frame(minWidth: 560, minHeight: 360)
            .navigationTitle("Data of \(session.device.name)")
            // The window looks as the console does.
            .preferredColorScheme(.dark)
            .focusedSceneObject(reading)
            .toolbar {
                Toggle(isOn: $reading.paused) { Label("Pause", systemImage: "pause.fill") }
                    .help("Keeps the text as it is, to select and copy from it")
                Button {
                    NSPasteboard.general.clearContents()
                    NSPasteboard.general.setString(reading.text, forType: .string)
                } label: {
                    Label("Copy All", systemImage: "doc.on.doc")
                }
                .help("Copies the whole text")
                Button { reading.export() } label: { Label("Export", systemImage: "square.and.arrow.down") }
                    .help("Saves the text to a file")
            }
            .onAppear {
                reading.device = session.device.name
                session.startReading(every: utility.interval)
                refresh()
            }
            .onDisappear { session.stopReading() }
            .onChange(of: session.details.pictures) { refresh() }
            .onChange(of: reading.paused) { refresh() }
    }

    private func refresh() {
        if !reading.paused {
            reading.text = session.dataText(shownBy: session.watching ? try? utility.display?.get() : nil)
        }
    }
}

/// Hold output updates that would disturb a mouse selection while AppKit tracks it.
class ConsoleTextView: NSTextView {
    private(set) var trackingSelection = false

    override func mouseDown(with event: NSEvent) {
        trackingSelection = true
        defer { trackingSelection = false }
        super.mouseDown(with: event)
    }
}

/// Read-only text in a fixed-width font that can be selected, and stays where it was scrolled to when it changes.
private struct SelectableText: NSViewRepresentable {
    let text: String
    let font: NSFont

    func makeNSView(context: Context) -> NSScrollView {
        let scroll = NSScrollView()
        scroll.hasVerticalScroller = true
        scroll.backgroundColor = TerminalLook.background
        // TextKit 1 scrolls and extends the selection while the mouse button is held.
        let view = ConsoleTextView(usingTextLayoutManager: false)
        view.autoresizingMask = [.width]
        view.isHorizontallyResizable = false
        view.isVerticallyResizable = true
        view.maxSize = NSSize(width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
        view.textContainer?.widthTracksTextView = true
        view.isEditable = false
        view.isSelectable = true
        view.usesFontPanel = false
        view.font = font
        view.textColor = TerminalLook.text
        view.backgroundColor = TerminalLook.background
        view.textContainerInset = TerminalLook.inset
        scroll.documentView = view
        return scroll
    }

    func updateNSView(_ scroll: NSScrollView, context: Context) {
        guard let view = scroll.documentView as? ConsoleTextView else { return }
        if view.font != font {
            view.font = font
        }
        guard !view.trackingSelection, view.string != text else { return }
        let origin = scroll.contentView.bounds.origin
        view.string = text
        scroll.contentView.scroll(to: origin)
        scroll.reflectScrolledClipView(scroll.contentView)
    }
}

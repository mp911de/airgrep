import SwiftUI

/// One display variant, loaded from the library the application carries for it.
/// A library stays loaded: its user interface is built once and shows whatever
/// connection it is told to follow.
@MainActor
final class DisplayLibrary {
    struct Failure: Error {
        let reason: String
    }

    let api: airgrep_display_api
    var width: Int { Int(api.width) }
    var height: Int { Int(api.height) }

    private static let prefix = "display-"
    private static var loaded: [String: DisplayLibrary] = [:]
    /// The library folder of the application.
    private static let folder = Bundle.main.privateFrameworksPath ?? ""

    /// The variants the application carries, by the names of their libraries.
    static let variants: [String] = {
        let files = (try? FileManager.default.contentsOfDirectory(atPath: folder)) ?? []
        return files.filter { $0.hasPrefix(prefix) && $0.hasSuffix(".dylib") }
            .map { String($0.dropFirst(prefix.count).dropLast(".dylib".count)) }
            .sorted().reversed()
    }()

    static func load(_ variant: String) -> Result<DisplayLibrary, Failure> {
        if let library = loaded[variant] {
            return .success(library)
        }
        let file = "\(prefix)\(variant).dylib"
        guard let handle = dlopen("\(folder)/\(file)", RTLD_NOW | RTLD_LOCAL) else {
            return .failure(Failure(reason: dlerror().map { String(cString: $0) } ?? "\(file) cannot be loaded."))
        }
        guard let library = DisplayLibrary(handle) else {
            dlclose(handle)
            return .failure(Failure(reason: "\(file) has an incompatible display interface."))
        }
        loaded[variant] = library
        return .success(library)
    }

    private init?(_ handle: UnsafeMutableRawPointer) {
        guard let symbol = dlsym(handle, "airgrep_display_get_api") else { return nil }
        let getAPI = unsafeBitCast(symbol, to: airgrep_display_getter_ptr.self)
        guard let api = getAPI(AIRGREP_DISPLAY_ABI, MemoryLayout<airgrep_display_api>.size)?.pointee,
              api.width > 0, api.height > 0, Int(api.width) <= Int.max / 4 / Int(api.height),
              api.follow != nil, api.step != nil, api.pixels != nil, api.touch != nil,
              api.muted != nil, api.explained != nil
        else {
            return nil
        }
        self.api = api
    }

    /// Points on screen per panel pixel: a small panel is shown larger.
    var scale: CGFloat { width < 400 ? 2 : 1 }
}

struct RemoteDisplayWindow: View {
    static let id = "remote-display"

    @EnvironmentObject private var utility: Utility

    var body: some View {
        if let session = utility.session {
            Group {
                switch utility.display {
                case .success(let library):
                    Watched(session: session, library: library)
                        // Another conversation is another display, from its startup screen on.
                        // A panel keeps the library it was made with.
                        .id([ObjectIdentifier(session), ObjectIdentifier(library)])
                case .failure(let failure):
                    ContentUnavailableView(
                        "Display Unavailable", systemImage: "rectangle.slash", description: Text(failure.reason)
                    )
                    .frame(width: 480, height: 320)
                case nil:
                    Color.clear.frame(width: 320, height: 200)
                }
            }
            .task(id: utility.variant) { utility.loadDisplay() }
        } else {
            EmptyWindow(id: Self.id)
        }
    }
}

/// The display of a session.
private struct Watched: View {
    @EnvironmentObject private var utility: Utility
    @ObservedObject var session: Session
    let library: DisplayLibrary

    var body: some View {
        Panel(session: session, library: library, clicks: utility.clicks)
            .overlay { if session.details.phase == .ended { ended } }
            .frame(width: CGFloat(library.width) * library.scale, height: CGFloat(library.height) * library.scale)
            .navigationTitle(session.device.name)
            .onAppear { session.watch(every: utility.interval) }
            .onDisappear { session.unwatch() }
    }

    /// Over the last picture, so it is not taken for the device as it is now.
    private var ended: some View {
        VStack(spacing: 10) {
            Image(systemName: "cable.connector.slash").font(.system(size: 36))
            Text(session.asked == nil ? "Disconnected" : "Restarting").font(.title2)
            if session.asked == nil {
                Text(session.details.error).font(.callout).multilineTextAlignment(.center)
            }
            Button("Connect Again") { utility.connect(session.device) }
        }
        .padding(24)
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(.regularMaterial)
    }
}

private struct Panel: NSViewRepresentable {
    let session: Session
    let library: DisplayLibrary
    let clicks: Bool

    func makeNSView(context: Context) -> PanelView { PanelView(session, library) }
    func updateNSView(_ view: PanelView, context: Context) { view.clicks = clicks }
    static func dismantleNSView(_ view: PanelView, coordinator: ()) { view.stop() }
}

/// Shows the pixels of a display and sends the pointer back as its finger.
final class PanelView: NSView {
    private let session: Session
    private let library: DisplayLibrary
    private var timer: Timer?
    private var resignation: NSObjectProtocol?
    private var last = CACurrentMediaTime()
    /// Time that passed and was not handed to the display yet, below a millisecond.
    private var remainder = 0.0
    /// The finger is down. It lifts on mouse up, when the pointer leaves or the window loses focus,
    /// and then stays up until the next mouse down, however the mouse is dragged on.
    private var pressed = false
    private var image: CGImage?
    /// Whether a press sounds, as the user chose it for this computer.
    var clicks = true

    init(_ session: Session, _ library: DisplayLibrary) {
        self.session = session
        self.library = library
        super.init(frame: .zero)
        addTrackingArea(NSTrackingArea(rect: .zero, options: [.mouseEnteredAndExited, .activeAlways, .inVisibleRect], owner: self))
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) { fatalError("not in a nib") }

    override var isFlipped: Bool { true }
    override func acceptsFirstMouse(for event: NSEvent?) -> Bool { true }

    /// The display runs while its view is in a window, and stops reading pictures when it is taken out.
    override func viewDidMoveToWindow() {
        stop()
        guard let window else {
            session.unwatch()
            return
        }
        last = CACurrentMediaTime()
        let timer = Timer(timeInterval: 1.0 / 60, repeats: true) { [weak self] _ in
            MainActor.assumeIsolated { self?.tick() }
        }
        // Common modes, so the display keeps running while a menu is open or a window is dragged.
        RunLoop.main.add(timer, forMode: .common)
        self.timer = timer
        resignation = NotificationCenter.default.addObserver(
            forName: NSWindow.didResignKeyNotification, object: window, queue: .main
        ) { [weak self] _ in
            MainActor.assumeIsolated { self?.lift() }
        }
    }

    func stop() {
        timer?.invalidate()
        timer = nil
        if let resignation {
            NotificationCenter.default.removeObserver(resignation)
        }
        resignation = nil
        lift()
    }

    @MainActor private func tick() {
        session.follow(on: library)
        let now = CACurrentMediaTime()
        remainder += (now - last) * 1000
        last = now
        let whole = remainder.rounded(.down)
        remainder -= whole
        if library.api.step(UInt32(whole)) || image == nil {
            capture()
        }
    }

    override func draw(_ dirtyRect: NSRect) {
        guard let image, let context = NSGraphicsContext.current?.cgContext else { return }
        // One panel pixel is a block of one colour, at any size.
        context.interpolationQuality = .none
        // The view counts from the top, an image from the bottom.
        context.translateBy(x: 0, y: bounds.height)
        context.scaleBy(x: 1, y: -1)
        context.draw(image, in: bounds)
    }

    private func capture() {
        guard let pixels = library.api.pixels() else { return }
        let bytes = library.width * library.height * 4
        // Blue, green, red and one unused byte: a little endian word with the unused byte first.
        let layout = CGBitmapInfo(rawValue: CGImageAlphaInfo.noneSkipFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue)
        guard let provider = CGDataProvider(data: Data(bytes: pixels, count: bytes) as CFData),
              let space = CGColorSpace(name: CGColorSpace.sRGB)
        else { return }
        image = CGImage(
            width: library.width, height: library.height, bitsPerComponent: 8, bitsPerPixel: 32,
            bytesPerRow: library.width * 4, space: space, bitmapInfo: layout, provider: provider,
            decode: nil, shouldInterpolate: false, intent: .defaultIntent)
        needsDisplay = true
    }

    /// The finger at the place of the pointer, in panel pixels.
    private func touch(at location: NSPoint, down: Bool) {
        guard bounds.width > 0, bounds.height > 0 else { return }
        let point = convert(location, from: nil)
        library.api.touch(
            Int32(point.x / bounds.width * CGFloat(library.width)),
            Int32(point.y / bounds.height * CGFloat(library.height)), down)
    }

    private func lift() {
        guard pressed else { return }
        pressed = false
        touch(at: window?.mouseLocationOutsideOfEventStream ?? .zero, down: false)
    }

    override func mouseDown(with event: NSEvent) {
        pressed = true
        touch(at: event.locationInWindow, down: true)
        // One click per press, as the buzzer of the device gives, unless its mute slider is on.
        if clicks, !library.api.muted() {
            Buzzer.shared.click()
        }
    }

    override func mouseDragged(with event: NSEvent) {
        if pressed {
            touch(at: event.locationInWindow, down: true)
        }
    }

    override func mouseUp(with event: NSEvent) { lift() }
    override func mouseExited(with event: NSEvent) { lift() }
}

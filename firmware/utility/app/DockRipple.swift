import AppKit

/// The ripple of the startup logo of the device, once, on the icon in the Dock when the application
/// starts: the dots swell and turn green as a ring from the plane passes them.
@MainActor
enum DockRipple {
    /// The wait for the icon to stand still after its launch, and the time between two pictures. The tuning knobs.
    static let delay = Duration.milliseconds(300), frame = Duration.milliseconds(33)

    private static var played = false

    static func playOnce() {
        guard !played else { return }
        played = true
        guard !NSWorkspace.shared.accessibilityDisplayShouldReduceMotion else { return }
        Task {
            try? await Task.sleep(for: delay)
            let tile = NSApp.dockTile
            // An install has the icon already.
            guard tile.contentView == nil else { return }
            let view = DockRippleView(frame: NSRect(origin: .zero, size: tile.size))
            tile.contentView = view
            let start = ContinuousClock.now
            while tile.contentView === view, view.ms < DockRippleView.length {
                view.ms = Int((ContinuousClock.now - start) / .milliseconds(1))
                tile.display()
                try? await Task.sleep(for: frame)
            }
            if tile.contentView === view {
                tile.contentView = nil
                tile.display()
            }
        }
    }
}

/// The icon of the application with its dots at one moment of the ripple.
final class DockRippleView: NSView {
    /// The time since the ring left the plane.
    var ms = 0
    var icon = NSImage.dockIcon

    /// The ripple of `widgets/logo/logo.c`, by its names there: RIPPLE_MS, RIPPLE_STEP_MS,
    /// RIPPLE_LOOSE_MS, PEAK and REST as shares of a round, PEAK_SCALE.
    private static let round = 2600.0, step = 200.0, loose = 90, peak = 143.0 / 1024, rest = 369.0 / 1024
    private static let peakScale = 358.0 / 256
    private static let grid = 4, plane = (column: 2, row: 1)
    /// One round reaches the last dot and lets it rest. The grid rests from then on.
    static let length = 1700

    /// The dots of `assets/logo.svg`, as `icon.swift` puts them on the canvas of the icon and the
    /// system puts that canvas into the tile. Parts of the tile, from its top left.
    private static let first = CGPoint(x: 0.2868, y: 0.2854), pitch = 0.1428, radius = 0.033
    private static let resting = NSColor(srgbRed: 0.682, green: 0.682, blue: 0.698, alpha: 1)
    private static let green = NSColor(srgbRed: 0.188, green: 0.82, blue: 0.345, alpha: 1)

    override func draw(_ dirtyRect: NSRect) {
        icon.draw(in: bounds)
        let side = bounds.width
        for row in 0..<Self.grid {
            for column in 0..<Self.grid where (column, row) != Self.plane {
                let swell = Self.swell(column, row, at: ms)
                guard swell > 0 else { continue }
                let radius = side * Self.radius * (1 + (Self.peakScale - 1) * swell)
                let centre = CGPoint(x: side * (Self.first.x + Self.pitch * Double(column)),
                                     y: side * (1 - Self.first.y - Self.pitch * Double(row)))
                (Self.resting.blended(withFraction: swell, of: Self.green) ?? Self.green).setFill()
                NSBezierPath(ovalIn: NSRect(x: centre.x - radius, y: centre.y - radius, width: 2 * radius, height: 2 * radius)).fill()
            }
        }
    }

    /// How far a dot is swollen, 0 at rest and 1 at its peak. A dot is a little off the ring, by its place.
    private static func swell(_ column: Int, _ row: Int, at ms: Int) -> Double {
        let distance = hypot(Double(column - plane.column), Double(row - plane.row))
        let off = (column * 7 + row * 13) % 5 * loose / 2 - loose
        let since = Double(ms - Int((distance * step).rounded()) - off) / round
        func ease(_ share: Double) -> Double { share * share * (3 - 2 * share) }
        if since < 0 || since >= rest { return 0 }
        return since < peak ? ease(since / peak) : 1 - ease((since - peak) / (rest - peak))
    }
}

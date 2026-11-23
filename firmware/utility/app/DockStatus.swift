import AppKit

/// The icon in the Dock shows how far an install is, for a user who looks elsewhere.
@MainActor
enum DockStatus {
    /// Nil for the plain icon.
    static func show(progress: Double?) {
        let tile = NSApp.dockTile
        // Every block of an install reports. The bar moves by whole percents.
        let share = progress.map { (min(max($0, 0), 1) * 100).rounded() / 100 }
        let shown = (tile.contentView as? DockTileView)?.share
        guard share != shown else { return }
        if let share {
            let view = tile.contentView as? DockTileView ?? DockTileView(frame: NSRect(origin: .zero, size: tile.size))
            view.share = share
            tile.contentView = view
        } else {
            tile.contentView = nil
        }
        tile.display()
    }
}

extension NSImage {
    /// The icon as the Dock shows it, in the shape the system gives it.
    static var dockIcon: NSImage { NSWorkspace.shared.icon(forFile: Bundle.main.bundlePath) }
}

/// The icon of the application with a thin bar across its foot, as the Finder shows a copy.
final class DockTileView: NSView {
    var share = 0.0
    var icon = NSImage.dockIcon

    /// The bar in parts of the tile. The tuning knobs.
    private static let inset = 0.19, height = 0.055, bottom = 0.17, outline = 0.008

    override func draw(_ dirtyRect: NSRect) {
        icon.draw(in: bounds)
        let side = bounds.width
        let track = NSRect(x: side * Self.inset, y: side * Self.bottom,
                           width: side * (1 - 2 * Self.inset), height: side * Self.height)
        let radius = track.height / 2
        NSColor(white: 0, alpha: 0.55).setFill()
        NSBezierPath(roundedRect: track, xRadius: radius, yRadius: radius).fill()
        // The track stays around the fill as an outline, so white reads on the white icon.
        let inner = track.insetBy(dx: side * Self.outline, dy: side * Self.outline)
        var filled = inner
        filled.size.width = max(inner.height, inner.width * share)
        NSColor.white.setFill()
        NSBezierPath(roundedRect: filled, xRadius: inner.height / 2, yRadius: inner.height / 2).fill()
    }
}

import AppKit
import CoreText

/// Pictures the application draws itself, in one colour, for the system to tint as it tints a symbol.
enum Artwork {
    /// An airgrep on a cable: the paper plane of the logo, and the plug of `cable.connector`.
    @MainActor static let device = template(NSSize(width: 100, height: 100), draw: drawDevice)
    /// The words of the logo, "air" and "grep" in its pill.
    @MainActor static let logo = template(NSSize(width: logoWidth, height: 80), draw: drawLogo)

    /// The tuning knobs of the device picture, in parts of its side.
    private static let stroke = 0.075, plugWidth = 0.20, plugAngle = CGFloat.pi / 6
    private static let cableEnd = CGPoint(x: 0.36, y: 0.57)
    /// The logo in the units of `assets/logo.svg`, from the "a" to the end of the pill.
    private static let logoLeft = 121.0, logoWidth = 251.5, logoTop = 40.0

    /// The picture is drawn anew at every size it is shown in.
    private static func template(_ size: NSSize, draw: @escaping @Sendable (CGContext, NSSize) -> Void) -> NSImage {
        let image = NSImage(size: size, flipped: true) { rect in
            guard let context = NSGraphicsContext.current?.cgContext else { return false }
            draw(context, rect.size)
            return true
        }
        image.isTemplate = true
        return image
    }

    private static func plane(_ context: CGContext, _ box: CGRect) {
        let s = box.width
        func point(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: box.minX + x * s, y: box.minY + y * s) }
        context.move(to: point(1, 0))
        context.addLine(to: point(0, 0.39))
        context.addLine(to: point(0.439, 0.561))
        context.addLine(to: point(0.61, 1))
        context.closePath()
        context.move(to: point(0.439, 0.561))
        context.addLine(to: point(1, 0))
        context.strokePath()
    }

    private static func drawDevice(_ context: CGContext, _ size: NSSize) {
        let s = size.width
        context.setLineWidth(stroke * s)
        context.setLineJoin(.round)
        context.setLineCap(.round)
        plane(context, CGRect(x: 0.36 * s, y: 0.05 * s, width: 0.59 * s, height: 0.59 * s))
        let end = CGPoint(x: cableEnd.x * s, y: cableEnd.y * s)
        context.move(to: CGPoint(x: 0.60 * s, y: 0.40 * s))
        context.addCurve(
            to: end, control1: CGPoint(x: 0.52 * s, y: 0.56 * s),
            control2: CGPoint(x: end.x + sin(plugAngle) * 0.19 * s, y: end.y - cos(plugAngle) * 0.19 * s))
        context.strokePath()

        guard let symbol = connector else { return }
        // The plug hangs on the cable with its tip away from the plane.
        let part = plug, from = NSRect(
            x: part.minX * symbol.size.width, y: (1 - part.maxY) * symbol.size.height,
            width: part.width * symbol.size.width, height: part.height * symbol.size.height)
        let width = plugWidth * s, height = width * from.height / from.width
        context.translateBy(x: end.x, y: end.y)
        context.rotate(by: plugAngle + .pi)
        symbol.draw(in: NSRect(x: -width / 2, y: -height + 0.01 * s, width: width, height: height), from: from,
                    operation: .sourceOver, fraction: 1, respectFlipped: true, hints: nil)
    }

    /// The symbol in one solid colour. By itself its tip is lighter than its body.
    private static var connector: NSImage? {
        NSImage(systemSymbolName: "cable.connector", accessibilityDescription: nil)?.withSymbolConfiguration(
            NSImage.SymbolConfiguration(pointSize: 100, weight: .regular).applying(.init(paletteColors: [.black])))
    }

    /// Where the plug is in `cable.connector`, without its cable, in parts of the symbol from its top left.
    private static let plug: CGRect = {
        let side = 256
        guard let symbol = connector,
              let pixels = NSBitmapImageRep(
                  bitmapDataPlanes: nil, pixelsWide: side, pixelsHigh: side, bitsPerSample: 8, samplesPerPixel: 4,
                  hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0),
              let context = NSGraphicsContext(bitmapImageRep: pixels), let data = pixels.bitmapData
        else { return CGRect(x: 0, y: 0, width: 1, height: 0.6) }
        NSGraphicsContext.saveGraphicsState()
        NSGraphicsContext.current = context
        symbol.draw(in: NSRect(x: 0, y: 0, width: side, height: side))
        NSGraphicsContext.restoreGraphicsState()
        // The ink of every row of pixels, from the top.
        let rows: [Range<Int>?] = (0..<side).map { y in
            let ink = (0..<side).filter { data[y * pixels.bytesPerRow + $0 * 4 + 3] > 127 }
            return ink.first.map { $0..<ink.last! + 1 }
        }
        let widest = rows.compactMap { $0?.count }.max() ?? 0
        // The cable is the narrow run below the body.
        guard let top = rows.firstIndex(where: { $0 != nil }),
              let bottom = rows.lastIndex(where: { ($0?.count ?? 0) * 10 > widest * 4 }) else {
            return CGRect(x: 0, y: 0, width: 1, height: 0.6)
        }
        let ink = rows[top...bottom].compactMap { $0 }
        let left = ink.map(\.lowerBound).min() ?? 0, right = ink.map(\.upperBound).max() ?? side
        let unit = CGFloat(side)
        return CGRect(x: CGFloat(left) / unit, y: CGFloat(top) / unit,
                      width: CGFloat(right - left) / unit, height: CGFloat(bottom + 1 - top) / unit)
    }()

    private static func drawLogo(_ context: CGContext, _ size: NSSize) {
        let scale = size.height / 80
        context.scaleBy(x: scale, y: scale)
        context.translateBy(x: -logoLeft, y: -logoTop)
        let font = CTFontCreateWithName("HelveticaNeue-Medium" as CFString, 64, nil)
        context.addPath(CGPath(roundedRect: CGRect(x: 204.85, y: 42, width: 165.63, height: 76),
                               cornerWidth: 38, cornerHeight: 38, transform: nil))
        // Even-odd: "grep" is cut out of the pill, "air" stands alone.
        word("grep", at: CGPoint(x: 222.61, y: 96), font, into: context)
        word("air", at: CGPoint(x: 123.17, y: 96), font, into: context)
        context.fillPath(using: .evenOdd)
    }

    /// Adds the outlines of a word, with the letter spacing of the logo, to the path of the context.
    private static func word(_ text: String, at baseline: CGPoint, _ font: CTFont, into context: CGContext) {
        let line = CTLineCreateWithAttributedString(NSAttributedString(
            string: text, attributes: [.font: font, .kern: -1.28]))
        for run in CTLineGetGlyphRuns(line) as? [CTRun] ?? [] {
            let count = CTRunGetGlyphCount(run)
            var glyphs = [CGGlyph](repeating: 0, count: count), places = [CGPoint](repeating: .zero, count: count)
            CTRunGetGlyphs(run, CFRange(), &glyphs)
            CTRunGetPositions(run, CFRange(), &places)
            for (glyph, place) in zip(glyphs, places) {
                var upright = CGAffineTransform(a: 1, b: 0, c: 0, d: -1, tx: baseline.x + place.x, ty: baseline.y - place.y)
                if let outline = CTFontCreatePathForGlyph(font, glyph, &upright) {
                    context.addPath(outline)
                }
            }
        }
    }
}

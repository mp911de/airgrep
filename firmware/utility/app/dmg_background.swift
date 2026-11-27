// Renders the Retina background TIFF for the macOS installer disk image:
// Cockpit Dark slate with radar rings, the dark logo, a flight-path arrow, and
// neutral backing pills so Finder labels stay legible in Light and Dark Mode.
//
//   swift dmg_background.swift logo-dark-1280.png dmg-background.tiff

import AppKit

let arguments = CommandLine.arguments
guard arguments.count == 3,
      let logo = NSImage(contentsOfFile: arguments[1])
else {
    FileHandle.standardError.write(Data("usage: dmg_background.swift logo-dark.png background.tiff\n".utf8))
    exit(2)
}

// Geometry in points, shared with dmg_settings.py (600x400 canvas, 104 pt icons at y=215).
let width: CGFloat = 600, height: CGFloat = 400
let leftX: CGFloat = 170, rightX: CGFloat = 430, iconCenterY: CGFloat = 215
// Finder places the visible icon slightly above its Iloc Y coordinate.
let visualCenterY: CGFloat = iconCenterY - 4

func rgb(_ r: CGFloat, _ g: CGFloat, _ b: CGFloat, _ a: CGFloat = 1) -> CGColor {
    CGColor(srgbRed: r / 255, green: g / 255, blue: b / 255, alpha: a)
}

func drawText(_ ctx: CGContext, _ text: String, at p: CGPoint, font: NSFont,
              color: NSColor, align: NSTextAlignment = .center) {
    let para = NSMutableParagraphStyle()
    para.alignment = align
    let attrs: [NSAttributedString.Key: Any] = [.font: font, .foregroundColor: color, .paragraphStyle: para]
    let attr = NSAttributedString(string: text, attributes: attrs)
    let size = attr.size()
    let x = align == .center ? p.x - size.width / 2 : p.x
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
    attr.draw(at: NSPoint(x: x, y: p.y))
    NSGraphicsContext.restoreGraphicsState()
}

func render(_ scale: Int) -> Data {
    let bitmap = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: Int(width) * scale, pixelsHigh: Int(height) * scale,
        bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false,
        colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    // Point size tells PNG/tiffutil the DPI (72 at 1x, 144 at 2x).
    bitmap.size = NSSize(width: width, height: height)
    let ctx = NSGraphicsContext(bitmapImageRep: bitmap)!.cgContext
    ctx.translateBy(x: 0, y: height)
    ctx.scaleBy(x: 1, y: -1)

    // Cockpit Dark slate background.
    ctx.setFillColor(rgb(21, 27, 35))
    ctx.fill(CGRect(x: 0, y: 0, width: width, height: height))

    // Concentric radar rings centered between the icons.
    ctx.saveGState()
    ctx.setStrokeColor(rgb(90, 108, 128, 0.28))
    ctx.setLineWidth(1.2)
    for (i, radius) in [CGFloat(72), CGFloat(126), CGFloat(180)].enumerated() {
        ctx.setLineDash(phase: 0, lengths: i == 1 ? [] : [4, 5])
        ctx.strokeEllipse(in: CGRect(x: width / 2 - radius, y: visualCenterY - radius,
                                     width: radius * 2, height: radius * 2))
    }
    ctx.restoreGState()

    // Logo + Utility title.
    let logoHeight: CGFloat = 44
    let logoWidth = logoHeight * (logo.size.width / logo.size.height)
    let startX = (width - (logoWidth + 12 + 86)) / 2
    NSGraphicsContext.saveGraphicsState()
    NSGraphicsContext.current = NSGraphicsContext(cgContext: ctx, flipped: true)
    ctx.interpolationQuality = .high
    logo.draw(in: NSRect(x: startX, y: 36, width: logoWidth, height: logoHeight))
    NSGraphicsContext.restoreGraphicsState()
    drawText(ctx, "Utility", at: CGPoint(x: startX + logoWidth + 12, y: 42),
             font: NSFont(name: "HelveticaNeue-Light", size: 30) ?? NSFont.systemFont(ofSize: 30, weight: .light),
             color: NSColor(calibratedRed: 0.72, green: 0.77, blue: 0.83, alpha: 1), align: .left)

    // Dashed green flight-path arrow from app icon to Applications.
    ctx.saveGState()
    ctx.setStrokeColor(rgb(48, 209, 88))
    ctx.setLineWidth(2.6)
    ctx.setLineCap(.round)
    ctx.setLineJoin(.round)
    ctx.setLineDash(phase: 0, lengths: [6, 6])
    ctx.move(to: CGPoint(x: 256, y: visualCenterY))
    ctx.addLine(to: CGPoint(x: 340, y: visualCenterY))
    ctx.strokePath()
    ctx.setLineDash(phase: 0, lengths: [])
    ctx.move(to: CGPoint(x: 332, y: visualCenterY - 9))
    ctx.addLine(to: CGPoint(x: 344, y: visualCenterY))
    ctx.addLine(to: CGPoint(x: 332, y: visualCenterY + 9))
    ctx.strokePath()
    ctx.restoreGState()

    // Neutral mid-slate backing pills centered behind Finder's icon labels so
    // both Light (black text) and Dark (white text) appearances stay legible.
    let pillY = iconCenterY + 52 + 8
    ctx.setFillColor(rgb(138, 148, 162, 0.72))
    for (cx, w) in [(leftX, CGFloat(132)), (rightX, CGFloat(100))] {
        ctx.addPath(CGPath(roundedRect: CGRect(x: cx - w / 2, y: pillY, width: w, height: 22),
                           cornerWidth: 6, cornerHeight: 6, transform: nil))
        ctx.fillPath()
    }

    // Placed above y=354 so it stays clear if Finder shows global path/status bars.
    drawText(ctx, "Drag airgrep Utility into Applications", at: CGPoint(x: width / 2, y: 318),
             font: NSFont.systemFont(ofSize: 12.5, weight: .regular),
             color: NSColor(calibratedRed: 0.62, green: 0.68, blue: 0.75, alpha: 1))

    return bitmap.representation(using: .png, properties: [:])!
}

func buildTiff() throws -> Int32 {
    let dir = FileManager.default.temporaryDirectory.appendingPathComponent("airgrep-dmg-\(getpid())")
    try FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: dir) }
    let img1x = dir.appendingPathComponent("background.png")
    let img2x = dir.appendingPathComponent("background@2x.png")
    try render(1).write(to: img1x)
    try render(2).write(to: img2x)
    let tiffutil = Process()
    tiffutil.executableURL = URL(fileURLWithPath: "/usr/bin/tiffutil")
    tiffutil.arguments = ["-cathidpicheck", img1x.path, img2x.path, "-out", arguments[2]]
    try tiffutil.run()
    tiffutil.waitUntilExit()
    return tiffutil.terminationStatus
}

exit(try buildTiff())

// Makes the application icon from the logo: its dot grid with the plane, on
// a white square that fills the canvas. macOS gives an icon its shape.
//
//   swift icon.swift logo-1280.png AppIcon.icns

import AppKit

// The grid within assets/logo-1280.png, in pixels from the top left.
let mark = CGRect(x: 4, y: 39, width: 200, height: 200)
// The mark keeps clear of the corners macOS rounds off a canvas of 1024.
let canvas = 1024.0, markSize = 640.0

let arguments = CommandLine.arguments
guard arguments.count == 3,
      let logo = NSImage(contentsOfFile: arguments[1])?.cgImage(forProposedRect: nil, context: nil, hints: nil),
      let grid = logo.cropping(to: mark)
else {
    FileHandle.standardError.write(Data("usage: icon.swift logo.png AppIcon.icns\n".utf8))
    exit(2)
}

func render(_ pixels: Int) -> Data {
    let bitmap = NSBitmapImageRep(
        bitmapDataPlanes: nil, pixelsWide: pixels, pixelsHigh: pixels, bitsPerSample: 8, samplesPerPixel: 4,
        hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 0, bitsPerPixel: 0)!
    let context = NSGraphicsContext(bitmapImageRep: bitmap)!.cgContext
    context.scaleBy(x: Double(pixels) / canvas, y: Double(pixels) / canvas)
    context.setFillColor(.white)
    context.fill(CGRect(x: 0, y: 0, width: canvas, height: canvas))
    context.interpolationQuality = .high
    let offset = (canvas - markSize) / 2
    context.draw(grid, in: CGRect(x: offset, y: offset, width: markSize, height: markSize))
    return bitmap.representation(using: .png, properties: [:])!
}

func buildIcon() throws -> Int32 {
    let set = FileManager.default.temporaryDirectory.appendingPathComponent("airgrep-\(getpid()).iconset")
    try FileManager.default.createDirectory(at: set, withIntermediateDirectories: true)
    defer { try? FileManager.default.removeItem(at: set) }
    for points in [16, 32, 128, 256, 512] {
        try render(points).write(to: set.appendingPathComponent("icon_\(points)x\(points).png"))
        try render(points * 2).write(to: set.appendingPathComponent("icon_\(points)x\(points)@2x.png"))
    }
    let iconutil = Process()
    iconutil.executableURL = URL(fileURLWithPath: "/usr/bin/iconutil")
    iconutil.arguments = ["--convert", "icns", "--output", arguments[2], set.path]
    try iconutil.run()
    iconutil.waitUntilExit()
    return iconutil.terminationStatus
}

exit(try buildIcon())

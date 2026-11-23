import AppKit
import UniformTypeIdentifiers

/// Text of a window, saved to a file the user names.
enum Export {
    /// The name a save panel suggests: the device, what the text is, and the minute it was saved.
    static func name(of device: String, _ what: String, at date: Date = Date(), in zone: TimeZone = .current) -> String {
        let format = DateFormatter()
        format.locale = Locale(identifier: "en_US_POSIX")
        format.timeZone = zone
        format.dateFormat = "yyyy-MM-dd_HH_mm"
        let device = device.lowercased().replacingOccurrences(of: " ", with: "-")
        return "\(device)-\(what)-\(format.string(from: date)).txt"
    }

    @MainActor
    static func save(_ text: String, as name: String) {
        let panel = NSSavePanel()
        panel.nameFieldStringValue = name
        panel.allowedContentTypes = [.plainText]
        func write(_ response: NSApplication.ModalResponse) {
            guard response == .OK, let file = panel.url else { return }
            do {
                try text.write(to: file, atomically: true, encoding: .utf8)
            } catch {
                NSAlert(error: error).runModal()
            }
        }
        if let window = NSApp.keyWindow {
            panel.beginSheetModal(for: window) { write($0) }
        } else {
            write(panel.runModal())
        }
    }
}

/// Whether a conversation keeps its page, judged by the listings of the bus since it ended.
struct Presence {
    /// A listing missed the device after the conversation ended.
    private(set) var left = false

    /// Takes one listing. True once the conversation is over for the user interface: it ended and
    /// its device left the bus, also when the device is back since. One that the user ended, by a
    /// restart or the bootloader, counts as left at once: the device is back within a second, which
    /// no listing sees. A page that is held, by an install or a window that shows the device, stays
    /// until it is let go.
    mutating func isOver(ended: Bool, asked: Bool, listed: Bool, held: Bool) -> Bool {
        left = ended && (left || asked || !listed)
        return left && !held
    }
}

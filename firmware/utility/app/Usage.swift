/// How much of something is used, as the three steps a gauge shows.
enum Usage: Int, CaseIterable {
    case low = 1, raised, high

    /// Low up to this many percent, raised up to the second, high above. The tuning knobs.
    static let lowLimit = 50, raisedLimit = 80

    init(percent: Int) {
        self = percent <= Self.lowLimit ? .low : percent <= Self.raisedLimit ? .raised : .high
    }

    /// The share of a total that is not free, in percent. Nothing of nothing is used.
    static func percent(free: UInt32, of total: UInt32) -> Int {
        total == 0 ? 0 : Int((UInt64(total - min(free, total)) * 100 + UInt64(total) / 2) / UInt64(total))
    }
}

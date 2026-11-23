#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include "conflicts/conflicts.hpp"

namespace conflicts {
// Which flights can reach the own ship within the look-ahead, the soonest
// first: their places in the traffic. Returns how many it wrote, no more
// than ranked holds and no more than kMaxAlertTargets.
size_t filter_traffic(const Flight& own, std::span<const Flight> traffic,
					  std::span<uint16_t> ranked);
}

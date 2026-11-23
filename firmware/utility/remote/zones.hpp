#pragma once

#include <cstdint>
#include <span>

#include "display/display_mapping.hpp"

// The time zone table of the device, firmware/tzdb, read on this computer. A
// zone without a rule in the table runs on UTC, as on the device. Without the
// table every function answers as for an unknown zone.
namespace remote::zones {
// Sorted by name, with their offsets at this moment.
std::span<const display::Zone> listed(int64_t utc_s);
// The zone a position lies in. Empty over an ocean.
const char* at(double latitude, double longitude);
display::mapping::Clock local(const char* zone, int64_t utc_s);
}

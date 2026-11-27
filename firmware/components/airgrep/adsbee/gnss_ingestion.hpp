#pragma once

#include "adsbee_link.hpp"

namespace adsbee {
void publish_fix(bool valid, AntennaStatus antenna);
void expire_fix(int64_t now_us);
}

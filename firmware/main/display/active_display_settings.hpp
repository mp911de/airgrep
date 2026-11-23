#pragma once

#include "display_settings.hpp"

// Writes commit synchronously to NVS. Readers on other tasks get a snapshot.
namespace display_settings {

void load();
display::Settings snapshot();
bool save(const display::Settings& settings);
}

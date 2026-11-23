#pragma once

#include <chrono>
#include <string>

#include "connection.hpp"
#include "display/view_state.hpp"

namespace remote::view {
// Actions has no context pointer. Each loaded display library owns one view.
display::Actions actions();
// Shows what the connection read since the last call. A device with another
// serial number starts from what a user has not chosen yet.
void follow(Connection& connection, const std::string& serial);
bool muted();
// What the display made of the last picture, as text for a human: what the
// user chose, for every report whether it shows and why not, and what the
// user asked of the display last. Empty before the first picture.
const std::string& explained();
}

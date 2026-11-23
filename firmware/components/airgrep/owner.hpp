#pragma once

// A raw pointer that owns what it points to. clang-tidy follows the name.
namespace gsl {
template <typename T>
using owner = T;
}

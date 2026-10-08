#pragma once
#include "model.hpp"

namespace clause::project {
// Decode filename UTF-8 strictly, rejecting overlong, surrogate, and truncated scalars.
std::u32string filename_scalars(std::string_view text, const Site &site);
} // namespace clause::project

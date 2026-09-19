#pragma once
#include "model.hpp"

namespace erlang_aot::project {
// Decode filename UTF-8 strictly, rejecting overlong, surrogate, and truncated scalars.
std::u32string filename_scalars(std::string_view text, const Site &site);
} // namespace erlang_aot::project

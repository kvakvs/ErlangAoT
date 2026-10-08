#pragma once
#include "loader.hpp"

namespace clause::project {
// Decode the complete schema into values independent of the parsed TOML tree.
Manifest decode(const Document &document, const Limits &limits = {});
} // namespace clause::project

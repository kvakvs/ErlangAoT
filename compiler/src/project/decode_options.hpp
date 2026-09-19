#pragma once
#include "schema.hpp"

namespace erlang_aot::project {
// Decode only supported typed frontend settings; semantic terms remain frontend-owned.
TargetOptions decode_options(const toml::node &node, const schema::Context &context);
} // namespace erlang_aot::project

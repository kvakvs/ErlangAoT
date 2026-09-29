#pragma once
#include "../semantic/types/inference.hpp"
#include <string>

namespace erlang_aot::cli {
// Describe a finite symbolic type with explicit display limits, without expanding recursive aliases.
std::string type_text(const semantic::types::Graph &graph, semantic::types::Id type);
// Preserve exact parameter relations while labeling top-valued implementation facts as unknown.
std::string fact_text(const semantic::types::Inference &inferred, semantic::types::Fact fact);
} // namespace erlang_aot::cli

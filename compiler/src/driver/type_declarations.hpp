#pragma once
#include "../semantic/types/declarations.hpp"
#include <iosfwd>

namespace erlang_aot::cli {
// Print user-authored metadata without treating declared contracts as implementation facts.
void print_declared_types(std::ostream &output, const semantic::types::Registry &registry,
                          const semantic::Module &module);
} // namespace erlang_aot::cli

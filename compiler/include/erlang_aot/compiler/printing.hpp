#pragma once
#include <erlang_aot/compiler/preprocessor.hpp>
#include <iosfwd>

namespace erlang_aot {
// Write one expanded form as UTF-8 Erlang source, followed by a newline.
// Accepts semantic PreprocessorSession forms; retains sigils' original literal bodies.
void print_preprocessed(std::ostream &output, const OrdinaryForm &form);
} // namespace erlang_aot

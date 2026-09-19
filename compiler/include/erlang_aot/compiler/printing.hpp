#pragma once
#include <cstddef>
#include <erlang_aot/compiler/ast/module.hpp>
#include <erlang_aot/compiler/preprocessor.hpp>
#include <iosfwd>

namespace erlang_aot {
// Write one expanded form as UTF-8 Erlang source, followed by a newline.
// Accepts semantic PreprocessorSession forms; retains sigils' original literal bodies.
void print_preprocessed(std::ostream &output, const OrdinaryForm &form);
// Write the owned syntax as an indented tree, with scalar fields on each node's line.
// Deep trees use explicit depth labels after 64 levels to bound indentation cost.
// A scheduled-object budget also bounds repeated visits to shared syntax; exhaustion throws length_error.
void print_ast(std::ostream &output, const ast::Module &module, std::size_t visits = 4000000);
} // namespace erlang_aot

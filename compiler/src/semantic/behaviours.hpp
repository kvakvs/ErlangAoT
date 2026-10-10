#pragma once
#include "declarations.hpp"
#include <span>

namespace clause::semantic {
// OTP's predefined behaviour_info/1 as Erlang source for a module declaring -callback; empty when the module
// declares no callback or defines behaviour_info/1 itself.
std::string behaviour_info_source(const ast::Module &syntax);
// The module a -behaviour/-behavior attribute form names with an atom; null for any other form.
// Added for --print-inputs.
const ast::Atom *behaviour_name(const ast::Module &syntax, const ast::Form &form);
// Reject -callback beside a hand-written behaviour_info/1 as erl_lint does.
void index_callbacks(Module &module, const Reporter &out);
// Warn about the callbacks each module's behaviours require and it does not export (erl_lint check_behaviour).
void check_behaviours(std::span<const std::unique_ptr<Module>> modules, const Reporter &out);
} // namespace clause::semantic

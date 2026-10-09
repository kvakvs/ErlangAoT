#pragma once
#include "inference_bindings.hpp"

// Narrowing by uses (docs/semantic.md#inference): an operation that raises unless its operands have some types
// proves those types for the variables it used, on the normal path after it.
namespace clause::semantic::types {
// Narrow the variables an evaluated expression used to the types it requires of them; it returned, so they have them.
void narrow_uses(BindingFacts &bindings, const ast::Expression &expression);
} // namespace clause::semantic::types

#pragma once
#include "inference.hpp"
#include "lattice.hpp"
#include <optional>

// Facts of apply/2,3 calls (docs/semantic.md#inference): the arguments an apply's list holds, and the fun fact of the
// batch function apply(M, F, Args) names.
namespace clause::semantic::types {
// The argument facts of an apply's argument list: its elements' facts for a literal proper list, else from its fact
// (`[]`, a list of known positions, or `arity` copies of a proper list's element); none when they are unknown.
std::optional<std::vector<Fact>> applied_arguments(const Inference &inference, Lattice &lattice,
                                                   const ast::Module &syntax, const ast::ExprId &list,
                                                   std::optional<std::size_t> arity);
// The one arity every member of a fun fact has; none for several or unknown arities.
std::optional<std::size_t> fun_arity(Lattice &lattice, Id fact);

// The facts of the module and function name apply(M, F, Args) calls.
struct Named {
    Id module;
    Id name;
};

// The fun fact of the exported batch function `target`/`arity` (singleton atom facts); none for another target.
std::optional<Id> named_fun(Inference &inference, const Named &target, std::size_t arity);
} // namespace clause::semantic::types

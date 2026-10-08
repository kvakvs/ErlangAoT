#pragma once
#include "domain.hpp"
#include <string>

// Types of the semantic type graph as Erlang type syntax (docs/semantic.md#printing-types): integer(), 1..5,
// {ok, T}, [T, ...], #{K => V}, A | B. Predefined erlang types drop their module; recursive declarations stay named.
namespace clause::semantic::types {
// The Erlang type text of `type`; nesting and size are bounded by `budget` nodes, past which `...` stands in.
std::string type_source(const Graph &graph, Id type, std::size_t budget = 1024);
} // namespace clause::semantic::types

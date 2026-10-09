#pragma once
#include "domain.hpp"
#include <map>
#include <string>
#include <string_view>
#include <vector>

// Types of the semantic type graph as Erlang type syntax (docs/semantic.md#printing-types): integer(), 1..5,
// {ok, T}, [T, ...], #{K => V}, A | B. Predefined erlang types drop their module; recursive declarations stay named.
namespace clause::semantic::types {
// How printed types write term(), any value: `_`, which type syntax also reads as any().
inline constexpr std::string_view TERM_SOURCE = "_";
// The field names of tuple records by record name and tuple size (fields + 1), to print matching tuples as records.
using RecordFields = std::map<std::pair<std::string, std::size_t>, std::vector<std::string>>;

// The Erlang type text of `type`; nesting and size are bounded by `budget` nodes, past which `...` stands in. A
// tuple whose first element is the name of a record of `records` and whose size is that record's prints as it.
std::string type_source(const Graph &graph, Id type, std::size_t budget = 1024, const RecordFields *records = nullptr);
} // namespace clause::semantic::types

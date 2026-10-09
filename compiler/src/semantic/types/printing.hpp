#pragma once
#include "domain.hpp"
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Types of the semantic type graph as Erlang type syntax (docs/semantic.md#printing-types): integer(), 1..5,
// {ok, T}, [T, ...], #{K => V}, A | B. Predefined erlang types drop their module; recursive declarations stay named.
namespace clause::semantic::types {
// How printed types write term(), any value: `_`, which type syntax also reads as any().
inline constexpr std::string_view TERM_SOURCE = "_";
// Nodes a printed type shows before `...` stands in.
inline constexpr std::size_t DEFAULT_BUDGET = 1024;
// The field names of tuple records by record name and tuple size (fields + 1), to print matching tuples as records.
using RecordFields = std::map<std::pair<std::string, std::size_t>, std::vector<std::string>>;

// The Erlang type text of `type`; nesting and size are bounded by `budget` nodes, past which `...` stands in. A
// tuple whose first element is the name of a record of `records` and whose size is that record's prints as it.
std::string type_source(const Graph &graph, Id type, std::size_t budget = DEFAULT_BUDGET,
                        const RecordFields *records = nullptr);

// A function type to print: its inputs, its result, and the argument the result equals when only that is known.
struct FunctionText {
    std::vector<Id> inputs;
    Id result;
    std::optional<std::size_t> argument = std::nullopt;
};

// A dependent fact to print: the function name of its construct, its parameters' names and one function type per
// clause over them.
struct DependentText {
    std::string_view name;
    std::vector<std::string> parameters;
    std::vector<FunctionText> types;
};

// `$case_operator(X :: 1) -> one; (X :: _) -> other`: each clause's parameter facts, named, and its result; a result
// known only as equal to an argument prints as that argument's name in `names`.
std::string dependent_source(const Graph &graph, const DependentText &dependent, std::span<const std::string> names,
                             const RecordFields *records = nullptr);

// `(Inputs) -> Result; (Inputs) -> Result` of function types, as signatures show them after a function's name. A
// result known only as equal to an argument prints as that argument's name in `names` (else `_argumentN`), and so
// does that argument's input when it is any term.
std::string function_source(const Graph &graph, std::span<const FunctionText> types, std::span<const std::string> names,
                            const RecordFields *records = nullptr);
} // namespace clause::semantic::types

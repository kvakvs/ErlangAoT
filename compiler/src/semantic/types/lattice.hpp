#pragma once
#include "domain.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// The fact domain of type inference (docs/semantic.md#inference-domain): the facts inference states about values,
// how two facts join where control flow meets, and how a recursive result widens, within budgets that keep facts
// small. Facts are nodes of a type graph; categories are erlang built-in type applications, so facts print by their
// built-in names (boolean(), non_neg_integer(), string(), ...).
namespace clause::semantic::types {
// The lowest and highest integer of a range, as canonical decimals.
struct IntegerBounds {
    std::string_view low;
    std::string_view high;
};

// The budgets of the fact domain; going past one widens a fact soundly, never fails.
struct FactLimits {
    // Singleton atoms or integers a fact keeps before atoms become atom() and integers their range.
    std::size_t singletons = 8;
    // Members a fact union keeps before it becomes term().
    std::size_t members = 8;
    // Container nesting a fact keeps before an inner fact becomes term().
    std::size_t depth = 4;
    // Tuple elements and map keys a fact keeps before it becomes tuple() or map().
    std::size_t elements = 16;
};

// The numbers a fact holds, for arithmetic on facts.
struct Numbers {
    // Whether it holds integers, from `low` to `high` (canonical decimals; unbounded when missing).
    bool integers = false;
    std::optional<std::string> low;
    std::optional<std::string> high;
    // Whether it holds floats, and whether it holds values that are no numbers.
    bool floats = false;
    bool others = false;
};

class Lattice {
  public:
    // Facts live in `graph`, which owns their nodes.
    explicit Lattice(Graph &graph, FactLimits limits = {}) noexcept : graph_(graph), limits_(limits) {}

    // The least fact that holds both, within the budgets: the result of two clauses or branches.
    Id join(Id left, Id right);
    // The join of a recursive result's previous and next facts that stops it from growing for ever: integer bounds
    // that moved since the previous round become unbounded, so the result reaches its category.
    Id widen(Id previous, Id next);

    // Facts of values. An integer (canonical decimal) and an atom.
    Id integer(std::string_view decimal);
    Id atom(std::string_view name);
    // A built-in category by its name: integer, float, atom, pid, binary, ...
    Id category(std::string_view name);
    // The integers from `bounds.low` to `bounds.high` (low below high).
    Id range(IntegerBounds bounds);
    // A tuple, a proper list of `element` (never empty when `nonempty`), the empty list, a map with exactly the
    // keys of `fields` (keys and values alternate), a fun of `arity` returning `result`, and a bitstring of `base`
    // bits plus any multiple of `unit` bits (unit 0: exactly `base` bits).
    Id tuple(std::vector<Id> elements);
    Id list(Id element, bool nonempty);
    Id nil();
    Id map(std::vector<Id> fields);
    Id fun(std::size_t arity, Id result);
    Id bitstring(std::uint64_t base, std::uint64_t unit);

    // The integers from `low` to `high` as a fact: a singleton, a range or the category of an unbounded interval.
    Id interval(const std::optional<std::string> &low, const std::optional<std::string> &high);
    // The numbers `fact` holds, and whether it may hold the atom `name`.
    Numbers numbers(Id fact);
    bool holds_atom(Id fact, std::string_view name);

    // The facts of a graph and its budgets, for the helpers of lattice.cpp.
    Graph &graph() noexcept { return graph_; }

    const FactLimits &limits() const noexcept { return limits_; }

    // Join `members` found `depth` containers deep; past the depth budget the join is term().
    Id join(std::span<const Id> members, std::size_t depth);

  private:
    // An element fact cut to the depth budget: containers below it are term() past depth - 1 levels.
    Id within_depth(Id fact);
    // The fact with containers nested deeper than `levels` replaced by term().
    Id truncate(Id fact, std::size_t levels);

    Graph &graph_;
    FactLimits limits_;
};
} // namespace clause::semantic::types

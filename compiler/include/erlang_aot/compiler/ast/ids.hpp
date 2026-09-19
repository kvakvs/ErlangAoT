#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>

namespace erlang_aot::ast {
namespace detail {
struct Owner;
template <typename Value, typename Tag> class Arena;
} // namespace detail

// Category-specific handles cannot be default-constructed or forged by consumers.
template <typename Tag> class Id {
  public:
    bool operator==(const Id &) const = default;

  private:
    template <typename Value, typename Category> friend class detail::Arena;
    // Retain the identity token so another owner's allocation cannot reuse its address.
    std::shared_ptr<const detail::Owner> owner_;

    struct Slot {
        // Pair position with its generation to avoid ambiguous numeric constructor arguments.
        std::size_t index;
        std::uint64_t generation;
        bool operator==(const Slot &) const = default;
    };

    // Slot plus monotonic generation detects stale handles after transaction rollback.
    Slot slot_;

    // Only the corresponding arena creates handles after inserting a node.
    Id(std::shared_ptr<const detail::Owner> owner, Slot slot) : owner_(std::move(owner)), slot_(slot) {}
};

struct ExprTag;
struct PatternSyntaxTag;
struct TypeTag;
struct TermTag;
struct FormTag;
struct OriginTag;
using ExprId = Id<ExprTag>;
using PatternSyntaxId = Id<PatternSyntaxTag>;
using TypeId = Id<TypeTag>;
using TermId = Id<TermTag>;
using FormId = Id<FormTag>;
using OriginId = Id<OriginTag>;
} // namespace erlang_aot::ast

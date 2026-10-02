#include "structural_order.hpp"
#include "../memory/heap_object.hpp"
#include "integers.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
namespace {
using Pair = std::pair<Term, Term>;
constexpr std::size_t work_limit = 1'000'000;

// Preserve Erlang category order independently of representation tags or pointer addresses.
TermResult<unsigned> rank(const Term &term) {
    switch (term.kind()) {
    case TermKind::smallint:
    case TermKind::bignum:
        return 0;
    case TermKind::atom:
        return 1;
    case TermKind::tuple:
    case TermKind::empty_tuple:
        return 2;
    case TermKind::empty_list:
        return 3;
    case TermKind::list:
        return 4;
    default:
        return std::unexpected(TermError::invalid_encoding);
    }
}

// Exact host atom equality preserves runtime identity; Erlang ordering uses Unicode spelling.
int atoms(const Term &left, const Term &right, bool exact) {
    if (exact && left.word() != right.word()) {
        return left.word() < right.word() ? -1 : 1;
    }
    const auto lhs = left.atom_spelling().value();
    const auto rhs = right.atom_spelling().value();
    return lhs < rhs ? -1 : static_cast<int>(lhs > rhs);
}

// Immediates need no worklist allocation; compound cells are handled by the iterative dispatcher.
TermResult<int> scalar(const Term &left, const Term &right, bool exact) {
    if (left.is_atom()) {
        return atoms(left, right, exact);
    }
    if (left.is_integer()) {
        const auto lhs = integer_read(left).value();
        const auto rhs = integer_read(right).value();
        return lhs < rhs ? -1 : static_cast<int>(lhs > rhs);
    }
    return 0;
}

// Tuple arity precedes lexicographic field comparison; reverse scheduling visits the first field first.
TermResult<int> tuples(const Pair &values, std::vector<Pair> &pending, std::size_t remaining) {
    const auto lhs = values.first.tuple_size().value();
    const auto rhs = values.second.tuple_size().value();
    if (lhs != rhs) {
        return lhs < rhs ? -1 : 1;
    }
    if (lhs > remaining - std::min(remaining, pending.size())) {
        return std::unexpected(TermError::resource_limit);
    }
    for (std::size_t i = lhs; i > 0; --i) {
        pending.emplace_back(values.first.tuple_element(i - 1).value(), values.second.tuple_element(i - 1).value());
    }
    return 0;
}

// Cons comparison checks heads before arbitrary tails, retaining proper and improper list semantics.
TermResult<int> lists(const Pair &values, std::vector<Pair> &pending, std::size_t remaining) {
    if (remaining - std::min(remaining, pending.size()) < 2) {
        return std::unexpected(TermError::resource_limit);
    }
    pending.emplace_back(values.first.tail().value(), values.second.tail().value());
    pending.emplace_back(values.first.head().value(), values.second.head().value());
    return 0;
}

// Dispatch only after both source handles were validated; children inherit their live owning storage.
TermResult<int> step(const Pair &values, std::vector<Pair> &pending, std::size_t remaining, bool exact) {
    const auto lhs = rank(values.first);
    const auto rhs = rank(values.second);
    if (!lhs || !rhs) {
        return std::unexpected(TermError::invalid_encoding);
    }
    if (*lhs != *rhs) {
        return *lhs < *rhs ? -1 : 1;
    }
    if (values.first.is_tuple()) {
        return tuples(values, pending, remaining);
    }
    if (values.first.is_cons()) {
        return lists(values, pending, remaining);
    }
    return scalar(values.first, values.second, exact);
}

// Charge both processed and queued pairs; no source nesting consumes the native call stack.
TermResult<int> compare(const Term &left, const Term &right, bool exact) {
    std::vector<Pair> pending;
    const auto initial = step({left, right}, pending, work_limit - 1, exact);
    if (!initial || *initial != 0) {
        return initial;
    }
    std::size_t remaining = work_limit - 1;
    while (!pending.empty()) {
        if (remaining-- == 0) {
            return std::unexpected(TermError::resource_limit);
        }
        auto values = std::move(pending.back());
        pending.pop_back();
        const auto result = step(values, pending, remaining, exact);
        if (!result || *result != 0) {
            return result;
        }
    }
    return 0;
}
} // namespace

TermResult<int> structural_order(const Term &left, const Term &right, bool exact) noexcept {
    for (const auto *value : {&left, &right}) {
        if (const auto checked = TermAccess::validate(*value); !checked) {
            return std::unexpected(checked.error());
        }
    }
    try {
        return compare(left, right, exact);
    } catch (const std::bad_alloc &) {
        return std::unexpected(TermError::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(TermError::resource_limit);
    } catch (...) {
        return std::unexpected(TermError::invalid_encoding);
    }
}
} // namespace erlang_aot::runtime::detail

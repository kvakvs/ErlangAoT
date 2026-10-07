#include "structural_order.hpp"
#include "../memory/heap_object.hpp"
#include "bitstrings.hpp"
#include "floats.hpp"
#include "maps.hpp"
#include "records.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime::detail {
namespace {
struct Pair {
    // Each queued comparison owns its children; map keys always use exact ordering independently of values.
    Term left;
    Term right;
    bool exact;
};

// Preserve Erlang category order independently of representation tags or pointer addresses.
TermResult<unsigned> rank(const Term &term) {
    if (term.is_number()) {
        return 0;
    }
    switch (term.kind()) {
    case TermKind::atom:
        return 1;
    case TermKind::tuple:
    case TermKind::empty_tuple:
        return 2;
    case TermKind::native_record:
        return 3;
    case TermKind::map:
        return 4;
    case TermKind::empty_list:
        return 5;
    case TermKind::list:
        return 6;
    case TermKind::bitstring:
        return 7;
    default:
        return std::unexpected(TermError::invalid_encoding);
    }
}

// Exact host atom equality preserves runtime identity; Erlang ordering uses Unicode spelling.
int atoms(const Term &left, const Term &right, bool exact) {
    const auto lhs = left.atom_spelling().value();
    const auto rhs = right.atom_spelling().value();
    if (lhs != rhs) {
        return lhs < rhs ? -1 : 1;
    }
    if (exact && left.word() != right.word()) {
        return left.word() < right.word() ? -1 : 1;
    }
    return 0;
}

// Scalar comparisons allocate only when arbitrary integer intermediates need storage.
TermResult<int> scalar(const Pair &values) {
    if (values.left.is_atom()) {
        return atoms(values.left, values.right, values.exact);
    }
    if (values.left.is_number()) {
        return numeric_order(values.left, values.right, values.exact);
    }
    return 0;
}

// Tuple arity precedes lexicographic fields; reverse scheduling visits the first field first.
TermResult<int> tuples(const Pair &values, std::vector<Pair> &pending) {
    const auto lhs = values.left.tuple_size().value();
    const auto rhs = values.right.tuple_size().value();
    if (lhs != rhs) {
        return lhs < rhs ? -1 : 1;
    }
    for (std::size_t i = lhs; i > 0; --i) {
        pending.push_back(
            {values.left.tuple_element(i - 1).value(), values.right.tuple_element(i - 1).value(), values.exact});
    }
    return 0;
}

// Cons comparison checks heads before arbitrary tails, preserving proper and improper list semantics.
TermResult<int> lists(const Pair &values, std::vector<Pair> &pending) {
    pending.push_back({values.left.tail().value(), values.right.tail().value(), values.exact});
    pending.push_back({values.left.head().value(), values.right.head().value(), values.exact});
    return 0;
}

// Canonical storage orders exact keys; compare all keys before any values, after comparing map sizes.
TermResult<int> maps(const Pair &values, std::vector<Pair> &pending) {
    const auto lhs = values.left.map_size().value();
    const auto rhs = values.right.map_size().value();
    if (lhs != rhs) {
        return lhs < rhs ? -1 : 1;
    }
    for (std::size_t i = lhs; i > 0; --i) {
        pending.push_back(
            {map_entry(values.left, i - 1)->second, map_entry(values.right, i - 1)->second, values.exact});
    }
    for (std::size_t i = lhs; i > 0; --i) {
        pending.push_back({map_entry(values.left, i - 1)->first, map_entry(values.right, i - 1)->first, true});
    }
    return 0;
}

// Order two atom words of records by spelling.
int atom_words(const Pair &values, Word left, Word right) {
    return atoms(TermAccess::child(values.left, left).value(), TermAccess::child(values.right, right).value(), false);
}

// Definitions order by module, name, export flag (false first), then field count.
int identities(const Pair &values, const RecordDefinition &lhs, const RecordDefinition &rhs) {
    if (const auto order = atom_words(values, lhs.module, rhs.module); order != 0) {
        return order;
    }
    if (const auto order = atom_words(values, lhs.name, rhs.name); order != 0) {
        return order;
    }
    if (lhs.exported != rhs.exported) {
        return lhs.exported ? 1 : -1;
    }
    if (lhs.fields.size() != rhs.fields.size()) {
        return lhs.fields.size() < rhs.fields.size() ? -1 : 1;
    }
    return 0;
}

// After their identities, definitions order by field names in definition order.
int definitions(const Pair &values, const RecordDefinition &lhs, const RecordDefinition &rhs) {
    if (const auto order = identities(values, lhs, rhs); order != 0) {
        return order;
    }
    for (std::size_t i = 0; i < lhs.fields.size(); ++i) {
        if (const auto order = atom_words(values, lhs.fields[i], rhs.fields[i]); order != 0) {
            return order;
        }
    }
    return 0;
}

// Records compare their captured definitions, then their values in definition order, first field first.
TermResult<int> records(const Pair &values, std::vector<Pair> &pending) {
    const auto lhs = record_view(values.left).value();
    const auto rhs = record_view(values.right).value();
    if (lhs.definition != rhs.definition) {
        if (const auto order = definitions(values, *lhs.definition, *rhs.definition); order != 0) {
            return order;
        }
    }
    for (std::size_t i = lhs.values.size(); i > 0; --i) {
        pending.push_back({TermAccess::child(values.left, lhs.values[i - 1]).value(),
                           TermAccess::child(values.right, rhs.values[i - 1]).value(), values.exact});
    }
    return 0;
}

TermResult<int> same_rank(const Pair &values, std::vector<Pair> &pending);

// Dispatch only validated parents; extracted children inherit their live owning storage. One word names one term,
// so identical words are equal without a walk, as in ERTS.
TermResult<int> step(const Pair &values, std::vector<Pair> &pending) {
    if (values.left.word() == values.right.word()) {
        return 0;
    }
    const auto lhs = rank(values.left);
    const auto rhs = rank(values.right);
    if (!lhs || !rhs) {
        return std::unexpected(TermError::invalid_encoding);
    }
    if (*lhs != *rhs) {
        return *lhs < *rhs ? -1 : 1;
    }
    return same_rank(values, pending);
}

// Same-category containers queue their children; bitstrings and scalars compare at once.
TermResult<int> same_rank(const Pair &values, std::vector<Pair> &pending) {
    if (values.left.is_tuple()) {
        return tuples(values, pending);
    }
    if (values.left.is_cons()) {
        return lists(values, pending);
    }
    if (values.left.is_map()) {
        return maps(values, pending);
    }
    if (values.left.is_native_record()) {
        return records(values, pending);
    }
    if (values.left.is_bitstring()) {
        return bit_order(values.left, values.right);
    }
    return scalar(values);
}

// Walk pending pairs depth first, with no source-depth recursion.
TermResult<int> compare(const Term &left, const Term &right, bool exact) {
    std::vector<Pair> pending;
    const auto initial = step({left, right, exact}, pending);
    if (!initial || *initial != 0) {
        return initial;
    }
    while (!pending.empty()) {
        auto values = std::move(pending.back());
        pending.pop_back();
        const auto result = step(values, pending);
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

#include "../terms/structural_order.hpp"
#include "terms.hpp"
#include "typed.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <vector>

// The term-access builtins of the bridge without an inline service (docs/builtins.md): setelement/3,
// make_tuple/2,3, tuple_to_list/1, list_to_tuple/1, '++'/2 and '--'/2, with OTP's badarg rules.
// TODO(step 43A): each runs to completion; once the scheduler exists, work proportional to the input must run in
// bounded portions with rooted state (BEAM traps).
namespace erlang_aot::runtime::builtins {
namespace {
// A tuple size argument of make_tuple: a small integer in 0..MAX_TUPLE_ARITY.
std::size_t arity(std::int64_t size) {
    if (size < 0 || static_cast<std::uint64_t>(size) > MAX_TUPLE_ARITY) {
        bad_argument();
    }
    return static_cast<std::size_t>(size);
}

// setelement(Index, Tuple, Value): a copy of Tuple with element Index (1-based) replaced.
// TODO(step 43A): copying a large tuple should run in portions.
TermResult<Term> setelement(ProcessContext &context, std::int64_t index, TupleArgument tuple, const Term &value) {
    if (index < 1 || static_cast<std::uint64_t>(index) > tuple.elements.size()) {
        bad_argument();
    }
    tuple.elements[static_cast<std::size_t>(index - 1)] = value;
    return TermFactory(context).tuple(tuple.elements);
}

// make_tuple(Size, Value): Size copies of Value.
// TODO(step 43A): filling a large tuple should run in portions.
TermResult<Term> make_tuple(ProcessContext &context, std::int64_t size, const Term &value) {
    const std::vector<Word> words(arity(size), value.word());
    return TermFactory(context).tuple_words(words);
}

// One {Index, Value} entry of make_tuple/3's list: true after storing Value at Index in `words`.
bool place(const Term &entry, std::vector<Word> &words) {
    if (!entry.is_tuple() || entry.tuple_size().value_or(0) != 2) {
        return false;
    }
    const auto index = entry.tuple_element(0);
    const auto value = entry.tuple_element(1);
    const auto position = index ? small(index->word()) : std::nullopt;
    if (!value || !position || *position < 1 || static_cast<std::uint64_t>(*position) > words.size()) {
        return false;
    }
    words[static_cast<std::size_t>(*position - 1)] = value->word();
    return true;
}

// make_tuple(Size, Default, [{Index, Value}]): later entries replace earlier ones.
TermResult<Term> make_tuple_list(ProcessContext &context, std::int64_t size, const Term &value,
                                 const ListArgument &entries) {
    std::vector<Word> words(arity(size), value.word());
    if (!std::ranges::all_of(entries.elements, [&](const Term &entry) { return place(entry, words); })) {
        bad_argument();
    }
    return TermFactory(context).tuple_words(words);
}

// tuple_to_list(Tuple).
// TODO(step 43A): building a long list should run in portions.
TermResult<Term> tuple_to_list(ProcessContext &context, const TupleArgument &tuple) {
    return TermFactory(context).list(tuple.elements);
}

// list_to_tuple(List): a proper list of at most MAX_TUPLE_ARITY elements.
// TODO(step 43A): walking a long list should run in portions.
TermResult<Term> list_to_tuple(ProcessContext &context, const ListArgument &list) {
    if (list.elements.size() > MAX_TUPLE_ARITY) {
        bad_argument();
    }
    return TermFactory(context).tuple(list.elements);
}

// Left ++ Right: Left must be a proper list; [] ++ Right is Right whatever it is.
// TODO(step 43A): copy Left in portions, as OTP's append traps.
TermResult<Term> append(ProcessContext &context, const ListArgument &left, const Term &right) {
    return left.elements.empty() ? right : TermFactory(context).list(left.elements, right);
}

// Exact term order (=:= equal is 0) for subtracting; a comparison failure aborts the builtin.
bool before(const Term &left, const Term &right) {
    const auto order = detail::structural_order(left, right, true);
    if (!order) {
        throw std::runtime_error("list subtraction comparison failed");
    }
    return *order < 0;
}

// Elements to remove, sorted by exact order, each with its remaining count.
using Removals = std::vector<std::pair<Term, std::size_t>>;

// Group the right operand of -- into sorted distinct elements with counts.
Removals removals(std::vector<Term> items) {
    std::ranges::stable_sort(items, before);
    Removals result;
    for (auto &item : items) {
        if (result.empty() || before(result.back().first, item)) {
            result.emplace_back(std::move(item), 0);
        }
        ++result.back().second;
    }
    return result;
}

// Whether `item` is still to be removed; consumes one of its occurrences when it is.
bool consume(Removals &pending, const Term &item) {
    const auto found = std::ranges::lower_bound(pending, item, before, &Removals::value_type::first);
    if (found == pending.end() || before(item, found->first) || found->second == 0) {
        return false;
    }
    --found->second;
    return true;
}

// Left -- Right: both proper lists; each element of Right removes the first exactly equal element of Left.
// TODO(step 43A): length checks, the removal set and the copy should run in portions, as OTP's subtract traps.
TermResult<Term> subtract(ProcessContext &context, const ListArgument &left, ListArgument right) {
    if (right.elements.empty()) {
        return left.term;
    }
    auto pending = removals(std::move(right.elements));
    std::vector<Term> kept;
    kept.reserve(left.elements.size());
    for (const auto &item : left.elements) {
        if (!consume(pending, item)) {
            kept.push_back(item);
        }
    }
    return TermFactory(context).list(kept);
}

constexpr std::array TERM_ACCESS_BUILTINS{
    typed_entry<setelement>("erlang", "setelement"),
    typed_entry<make_tuple>("erlang", "make_tuple"),
    typed_entry<make_tuple_list>("erlang", "make_tuple"),
    typed_entry<tuple_to_list>("erlang", "tuple_to_list"),
    typed_entry<list_to_tuple>("erlang", "list_to_tuple"),
    typed_entry<append>("erlang", "++"),
    typed_entry<subtract>("erlang", "--"),
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> term_access_builtins() noexcept { return builtins::TERM_ACCESS_BUILTINS; }
} // namespace erlang_aot::runtime

#include "../terms/structural_order.hpp"
#include "support.hpp"
#include "terms.hpp"
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
// The elements of a proper list; none for an improper one.
TermResult<std::optional<std::vector<Term>>> elements(Term list) {
    std::vector<Term> result;
    while (list.is_cons()) {
        auto head = list.head();
        auto tail = list.tail();
        if (!head || !tail) {
            return std::unexpected(head ? tail.error() : head.error());
        }
        result.push_back(std::move(*head));
        list = std::move(*tail);
    }
    return list.is_nil() ? std::optional{std::move(result)} : std::nullopt;
}

// The elements of a proper list argument; none after raising badarg for anything else or recording a failure.
std::optional<std::vector<Term>> list_argument(ProcessContext &context, Word word) {
    const auto list = admit(context, word);
    if (!list) {
        return std::nullopt;
    }
    auto items = elements(*list);
    if (!items) {
        context.generated_calls().fail_service(detail::term_status(items.error()));
        return std::nullopt;
    }
    if (!*items) {
        badarg(context);
    }
    return std::move(*items);
}

// The elements of a tuple argument; none after raising badarg for anything else or recording a failure.
std::optional<std::vector<Term>> tuple_argument(ProcessContext &context, Word word) {
    const auto tuple = admit(context, word);
    if (!tuple) {
        return std::nullopt;
    }
    if (!tuple->is_tuple()) {
        badarg(context);
        return std::nullopt;
    }
    auto items = tuple->tuple_elements();
    if (!items) {
        context.generated_calls().fail_service(detail::term_status(items.error()));
        return std::nullopt;
    }
    return std::move(*items);
}

// A tuple size argument of make_tuple: a small integer in 0..MAX_TUPLE_ARITY.
std::optional<std::size_t> arity(Word word) {
    const auto value = small(word);
    if (!value || *value < 0 || static_cast<std::uint64_t>(*value) > MAX_TUPLE_ARITY) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(*value);
}

// setelement(Index, Tuple, Value): a copy of Tuple with element Index (1-based) replaced.
// TODO(step 43A): copying a large tuple should run in portions.
Word setelement(ProcessContext &context, Arguments arguments) {
    auto items = tuple_argument(context, arguments[1]);
    const auto value = items ? admit(context, arguments[2]) : std::nullopt;
    if (!value) {
        return 0;
    }
    const auto index = small(arguments[0]);
    if (!index || *index < 1 || static_cast<std::uint64_t>(*index) > items->size()) {
        return badarg(context);
    }
    (*items)[static_cast<std::size_t>(*index - 1)] = *value;
    return publish(context, TermFactory(context).tuple(*items));
}

// make_tuple(Size, Value): Size copies of Value.
// TODO(step 43A): filling a large tuple should run in portions.
Word make_tuple(ProcessContext &context, Arguments arguments) {
    const auto size = arity(arguments[0]);
    if (!size) {
        return badarg(context);
    }
    const std::vector<Word> words(*size, arguments[1]);
    return publish(context, TermFactory(context).tuple_words(words));
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
Word make_tuple_list(ProcessContext &context, Arguments arguments) {
    const auto size = arity(arguments[0]);
    const auto entries = size ? admit(context, arguments[2]) : std::nullopt;
    if (!size || !entries) {
        return size ? 0 : badarg(context);
    }
    auto items = elements(*entries);
    if (!items) {
        return publish(context, std::unexpected(items.error()));
    }
    std::vector<Word> words(*size, arguments[1]);
    if (!*items || !std::ranges::all_of(**items, [&](const Term &entry) { return place(entry, words); })) {
        return badarg(context);
    }
    return publish(context, TermFactory(context).tuple_words(words));
}

// tuple_to_list(Tuple).
// TODO(step 43A): building a long list should run in portions.
Word tuple_to_list(ProcessContext &context, Arguments arguments) {
    const auto items = tuple_argument(context, arguments[0]);
    return items ? publish(context, TermFactory(context).list(*items)) : 0;
}

// list_to_tuple(List): a proper list of at most MAX_TUPLE_ARITY elements.
// TODO(step 43A): walking a long list should run in portions.
Word list_to_tuple(ProcessContext &context, Arguments arguments) {
    const auto items = list_argument(context, arguments[0]);
    if (!items) {
        return 0;
    }
    if (items->size() > MAX_TUPLE_ARITY) {
        return badarg(context);
    }
    return publish(context, TermFactory(context).tuple(*items));
}

// Left ++ Right: Left must be a proper list; [] ++ Right is Right whatever it is.
// TODO(step 43A): copy Left in portions, as OTP's append traps.
Word append(ProcessContext &context, Arguments arguments) {
    const auto left = admit(context, arguments[0]);
    const auto right = left ? admit(context, arguments[1]) : std::nullopt;
    if (!right) {
        return 0;
    }
    if (left->is_nil()) {
        return right->word();
    }
    if (!left->is_cons()) {
        return badarg(context);
    }
    const auto items = list_argument(context, arguments[0]);
    return items ? publish(context, TermFactory(context).list(*items, *right)) : 0;
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
Word subtract(ProcessContext &context, Arguments arguments) {
    const auto left = list_argument(context, arguments[0]);
    const auto right = left ? list_argument(context, arguments[1]) : std::nullopt;
    if (!right) {
        return 0;
    }
    if (right->empty()) {
        return arguments[0];
    }
    auto pending = removals(*right);
    std::vector<Term> kept;
    kept.reserve(left->size());
    for (const auto &item : *left) {
        if (!consume(pending, item)) {
            kept.push_back(item);
        }
    }
    return publish(context, TermFactory(context).list(kept));
}

constexpr std::array TERM_ACCESS_BUILTINS{
    BuiltinEntry{"erlang", "setelement", 3, setelement},
    BuiltinEntry{"erlang", "make_tuple", 2, make_tuple},
    BuiltinEntry{"erlang", "make_tuple", 3, make_tuple_list},
    BuiltinEntry{"erlang", "tuple_to_list", 1, tuple_to_list},
    BuiltinEntry{"erlang", "list_to_tuple", 1, list_to_tuple},
    BuiltinEntry{"erlang", "++", 2, append},
    BuiltinEntry{"erlang", "--", 2, subtract},
};
} // namespace
} // namespace erlang_aot::runtime::builtins

namespace erlang_aot::runtime {
std::span<const BuiltinEntry> term_access_builtins() noexcept { return builtins::TERM_ACCESS_BUILTINS; }
} // namespace erlang_aot::runtime

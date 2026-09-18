#include "expression.hpp"
#include <algorithm>
#include <cmath>
#include <map>

namespace erlang_aot {
namespace {
using Values = std::vector<Value>;
using Guard = Value (*)(const Values &);
using Signature = std::pair<std::u32string_view, std::size_t>;

// Reject invalid Erlang types before indexing or operating on host containers.
const Value &typed(const Value &value, ValueKind kind) {
    if (value.kind != kind) {
        throw EvaluationFailure();
    }
    return value;
}

const Value &typed(Value &&, ValueKind) = delete;

bool proper_list(const Value &value) {
    return (value.kind == ValueKind::nil || value.kind == ValueKind::list) && !value.tail;
}

Value list_head(const Values &arguments) {
    const auto &value = typed(arguments[0], ValueKind::list);
    if (value.elements.empty()) {
        throw EvaluationFailure();
    }
    return value.elements.front();
}

Value list_tail(const Values &arguments) {
    const auto &value = typed(arguments[0], ValueKind::list);
    if (value.elements.empty()) {
        throw EvaluationFailure();
    }
    if (value.elements.size() == 1) {
        return value.tail ? *value.tail : Value{};
    }
    auto result = list({value.elements.begin() + 1, value.elements.end()});
    result.tail = value.tail;
    return result;
}

Value length(const Values &arguments) {
    if (!proper_list(arguments[0])) {
        throw EvaluationFailure();
    }
    return integer(arguments[0].elements.size());
}

Value element(const Values &arguments) {
    const auto &tuple = typed(arguments[1], ValueKind::tuple);
    const auto offset = index(arguments[0], tuple.elements.size());
    if (offset == 0) {
        throw EvaluationFailure();
    }
    return tuple.elements[offset - 1];
}

const Value *map_find(const Value &key, const Value &map) {
    typed(map, ValueKind::map);
    for (std::size_t i = 0; i < map.elements.size(); i += 2) {
        if (compare(key, map.elements[i], true) == 0) {
            return &map.elements[i + 1];
        }
    }
    return nullptr;
}

Value map_get(const Values &arguments) {
    const auto *value = map_find(arguments[0], arguments[1]);
    if (!value) {
        throw EvaluationFailure();
    }
    return *value;
}

Value size(const Values &arguments) {
    const auto &value = arguments[0];
    if (value.kind == ValueKind::tuple) {
        return integer(value.elements.size());
    }
    typed(value, ValueKind::bits);
    return integer(value.bits.size() / 8);
}

Value rounded(const Value &value, double (*operation)(double)) {
    if (value.kind == ValueKind::integer) {
        return value;
    }
    return integer(BigInt(operation(real(value))));
}

Value record(const Values &arguments) {
    if (arguments.size() < 2) {
        throw EvaluationFailure();
    }
    typed(arguments[1], ValueKind::atom);
    const auto &value = arguments[0];
    const auto count = arguments.size() == 3 ? index(arguments[2]) : value.elements.size();
    return boolean(value.kind == ValueKind::tuple && !value.elements.empty() && value.elements.size() == count &&
                   compare(value.elements[0], arguments[1], true) == 0);
}

Value binary_part(const Values &arguments) {
    const auto &binary = typed(arguments[0], ValueKind::bits);
    if (binary.bits.size() % 8 != 0) {
        throw EvaluationFailure();
    }
    auto position = arguments[1];
    auto count = arguments.back();
    if (arguments.size() == 2) {
        const auto &pair = typed(arguments[1], ValueKind::tuple);
        if (pair.elements.size() != 2) {
            throw EvaluationFailure();
        }
        count = pair.elements[1];
        position = pair.elements[0];
    }
    const auto total = binary.bits.size() / 8;
    const auto position_index = index(position, total);
    const bool backwards = integral(count) < 0;
    const auto length = index(backwards ? integer(-integral(count)) : count, total) * 8;
    const auto boundary = position_index * 8;
    if (length > (backwards ? boundary : binary.bits.size() - boundary)) {
        throw EvaluationFailure();
    }
    const auto start = backwards ? boundary - length : boundary;
    Value result;
    result.kind = ValueKind::bits;
    result.bits.assign(binary.bits.begin() + static_cast<std::ptrdiff_t>(start),
                       binary.bits.begin() + static_cast<std::ptrdiff_t>(start + length));
    return result;
}

// OTP's bounded integer predicate requires integer bounds as well as an integer candidate.
Value integer_range(const Values &a) {
    if (!std::ranges::all_of(a, [](const Value &value) { return value.kind == ValueKind::integer; })) {
        return boolean(false);
    }
    return boolean(compare(a[0], a[1]) >= 0 && compare(a[0], a[2]) <= 0);
}

// Any nonnegative integer arity is valid; only the matching external fun succeeds.
Value function_arity(const Values &arguments) {
    if (integral(arguments[1]) < 0) {
        throw EvaluationFailure();
    }
    return boolean(arguments[0].kind == ValueKind::function &&
                   compare(arguments[0].elements[1], arguments[1], true) == 0);
}

// This closed dispatch table is the pinned erl_internal:guard_bif/2 catalog.
const std::map<Signature, Guard> &guards() {
    static const std::map<Signature, Guard> table{
        {{U"abs", 1},
         [](const Values &a) {
             return a[0].kind == ValueKind::integer ? integer(a[0].integer < 0 ? -a[0].integer : a[0].integer)
                                                    : floating(std::abs(real(a[0])));
         }},
        {{U"binary_part", 2}, binary_part},
        {{U"binary_part", 3}, binary_part},
        {{U"bit_size", 1}, [](const Values &a) { return integer(typed(a[0], ValueKind::bits).bits.size()); }},
        {{U"byte_size", 1},
         [](const Values &a) { return integer((typed(a[0], ValueKind::bits).bits.size() + 7) / 8); }},
        {{U"ceil", 1}, [](const Values &a) { return rounded(a[0], std::ceil); }},
        {{U"floor", 1}, [](const Values &a) { return rounded(a[0], std::floor); }},
        {{U"round", 1}, [](const Values &a) { return rounded(a[0], std::round); }},
        {{U"trunc", 1}, [](const Values &a) { return rounded(a[0], std::trunc); }},
        {{U"float", 1}, [](const Values &a) { return floating(real(a[0])); }},
        {{U"element", 2}, element},
        {{U"hd", 1}, list_head},
        {{U"tl", 1}, list_tail},
        {{U"length", 1}, length},
        {{U"map_get", 2}, map_get},
        {{U"map_size", 1}, [](const Values &a) { return integer(typed(a[0], ValueKind::map).elements.size() / 2); }},
        {{U"is_map_key", 2}, [](const Values &a) { return boolean(map_find(a[0], a[1]) != nullptr); }},
        {{U"max", 2}, [](const Values &a) { return compare(a[0], a[1]) >= 0 ? a[0] : a[1]; }},
        {{U"min", 2}, [](const Values &a) { return compare(a[0], a[1]) <= 0 ? a[0] : a[1]; }},
        {{U"size", 1}, size},
        {{U"tuple_size", 1}, [](const Values &a) { return integer(typed(a[0], ValueKind::tuple).elements.size()); }},
        {{U"node", 0}, [](const Values &) { return atom(U"nonode@nohost"); }},
        {{U"node", 1},
         [](const Values &a) {
             typed(a[0], ValueKind::pid);
             return atom(U"nonode@nohost");
         }},
        {{U"self", 0},
         [](const Values &) {
             Value value;
             value.kind = ValueKind::pid;
             return value;
         }},
        {{U"is_integer", 3}, integer_range},
        {{U"is_record", 1}, record},
        {{U"is_record", 2}, record},
        {{U"is_record", 3}, record},
        {{U"is_function", 2}, function_arity}};
    return table;
}

// Type tests are independent of host runtime objects.
const std::map<std::u32string_view, bool (*)(const Value &)> &types() {
    static const std::map<std::u32string_view, bool (*)(const Value &)> table{
        {U"is_atom", [](const Value &v) { return v.kind == ValueKind::atom; }},
        {U"is_binary", [](const Value &v) { return v.kind == ValueKind::bits && v.bits.size() % 8 == 0; }},
        {U"is_bitstring", [](const Value &v) { return v.kind == ValueKind::bits; }},
        {U"is_boolean",
         [](const Value &v) { return v.kind == ValueKind::atom && (v.text == U"true" || v.text == U"false"); }},
        {U"is_float", [](const Value &v) { return v.kind == ValueKind::floating; }},
        {U"is_function", [](const Value &v) { return v.kind == ValueKind::function; }},
        {U"is_integer", [](const Value &v) { return v.kind == ValueKind::integer; }},
        {U"is_list", [](const Value &v) { return v.kind == ValueKind::list || v.kind == ValueKind::nil; }},
        {U"is_number", numeric},
        {U"is_map", [](const Value &v) { return v.kind == ValueKind::map; }},
        {U"is_pid", [](const Value &v) { return v.kind == ValueKind::pid; }},
        {U"is_port", [](const Value &v) { return v.kind == ValueKind::port; }},
        {U"is_reference", [](const Value &v) { return v.kind == ValueKind::reference; }},
        {U"is_tuple", [](const Value &v) { return v.kind == ValueKind::tuple; }}};
    return table;
}
} // namespace

bool guard_signature(std::u32string_view name, std::size_t arity) {
    return (arity == 1 && types().contains(name)) || guards().contains({name, arity});
}

Value guard_call(std::u32string_view name, const std::vector<Value> &arguments) {
    if (arguments.size() == 1) {
        if (const auto found = types().find(name); found != types().end()) {
            return boolean(found->second(arguments.front()));
        }
    }
    const auto found = guards().find({name, arguments.size()});
    if (found == guards().end()) {
        throw EvaluationFailure();
    }
    return found->second(arguments);
}
} // namespace erlang_aot

#include "value.hpp"
#include <algorithm>
#include <cmath>

namespace erlang_aot {
Value integer(BigInt number) {
    const BigInt magnitude = number < 0 ? -number : number;
    if (magnitude != 0 && boost::multiprecision::msb(magnitude) >= 1000000) {
        throw EvaluationLimit();
    }
    Value value;
    value.kind = ValueKind::integer;
    value.integer = std::move(number);
    return value;
}

Value atom(std::u32string text) {
    Value value;
    value.kind = ValueKind::atom;
    value.text = std::move(text);
    return value;
}

Value boolean(bool value) { return atom(value ? U"true" : U"false"); }

Value floating(double number) {
    if (!std::isfinite(number)) {
        throw EvaluationFailure();
    }
    Value value;
    value.kind = ValueKind::floating;
    value.real = number;
    return value;
}

Value list(std::vector<Value> values) {
    Value value;
    value.kind = values.empty() ? ValueKind::nil : ValueKind::list;
    value.elements = std::move(values);
    return value;
}

bool truth(const Value &value) { return value.kind == ValueKind::atom && value.text == U"true"; }

bool numeric(const Value &value) { return value.kind == ValueKind::integer || value.kind == ValueKind::floating; }

double real(const Value &value) {
    if (value.kind == ValueKind::floating) {
        return value.real;
    }
    if (value.kind == ValueKind::integer) {
        return value.integer.convert_to<double>();
    }
    throw EvaluationFailure();
}

const BigInt &integral(const Value &value) {
    if (value.kind != ValueKind::integer) {
        throw EvaluationFailure();
    }
    return value.integer;
}

std::size_t index(const Value &value, std::size_t maximum) {
    const auto &number = integral(value);
    if (number < 0 || number > maximum) {
        throw EvaluationFailure();
    }
    return number.convert_to<std::size_t>();
}

namespace {
// Preserve Erlang's global term ordering, where all numbers share the first rank.
int rank(ValueKind kind) {
    if (kind == ValueKind::floating) {
        return 0;
    }
    return static_cast<int>(kind);
}

template <class T> int ordered(const T &left, const T &right) {
    return left < right ? -1 : static_cast<int>(left > right);
}

// Avoid converting a large integer to double when comparing mixed numeric values.
int numeric_compare(const Value &left, const Value &right, bool exact) {
    if (left.kind == right.kind) {
        return left.kind == ValueKind::integer ? ordered(left.integer, right.integer) : ordered(left.real, right.real);
    }
    if (exact) {
        return ordered(left.kind, right.kind);
    }
    if (left.kind == ValueKind::floating) {
        return -numeric_compare(right, left, exact);
    }
    const BigInt truncated(right.real);
    const auto result = ordered(left.integer, truncated);
    if (result != 0) {
        return result;
    }
    const double fraction = right.real - std::trunc(right.real);
    return ordered(0.0, fraction);
}

// Compare tuple fields or binary bits in their original order.
int sequence_compare(const std::vector<Value> &left, const std::vector<Value> &right, bool exact) {
    for (std::size_t i = 0; i < std::min(left.size(), right.size()); ++i) {
        if (const auto result = compare(left[i], right[i], exact); result != 0) {
            return result;
        }
    }
    return ordered(left.size(), right.size());
}

// Reconstruct a list suffix only at the unequal-length comparison boundary.
Value tail(const Value &value, std::size_t skip) {
    if (skip == value.elements.size()) {
        return value.tail ? *value.tail : Value{};
    }
    auto result = list({value.elements.begin() + static_cast<std::ptrdiff_t>(skip), value.elements.end()});
    result.tail = value.tail;
    return result;
}

int list_compare(const Value &left, const Value &right, bool exact) {
    const auto count = std::min(left.elements.size(), right.elements.size());
    for (std::size_t i = 0; i < count; ++i) {
        if (const auto result = compare(left.elements[i], right.elements[i], exact); result != 0) {
            return result;
        }
    }
    return compare(tail(left, count), tail(right, count), exact);
}

// Map keys have exact equality and a deterministic ordering independent of insertion.
std::vector<std::size_t> keys(const Value &value) {
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < value.elements.size(); i += 2) {
        result.push_back(i);
    }
    std::ranges::sort(
        result, [&](std::size_t a, std::size_t b) { return compare(value.elements[a], value.elements[b], true) < 0; });
    return result;
}

int map_compare(const Value &left, const Value &right, bool exact) {
    if (left.elements.size() != right.elements.size()) {
        return ordered(left.elements.size(), right.elements.size());
    }
    const auto a = keys(left);
    const auto b = keys(right);
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (const auto result = compare(left.elements[a[i]], right.elements[b[i]], true); result != 0) {
            return result;
        }
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (const auto result = compare(left.elements[a[i] + 1], right.elements[b[i] + 1], exact); result != 0) {
            return result;
        }
    }
    return 0;
}
} // namespace

int compare(const Value &left, const Value &right, bool exact) {
    if (numeric(left) && numeric(right)) {
        return numeric_compare(left, right, exact);
    }
    if (left.kind != right.kind) {
        return ordered(rank(left.kind), rank(right.kind));
    }
    switch (left.kind) {
    case ValueKind::atom:
        return ordered(left.text, right.text);
    case ValueKind::tuple:
        if (left.elements.size() != right.elements.size()) {
            return ordered(left.elements.size(), right.elements.size());
        }
        return sequence_compare(left.elements, right.elements, exact);
    case ValueKind::map:
        return map_compare(left, right, exact);
    case ValueKind::list:
        return list_compare(left, right, exact);
    case ValueKind::bits:
        return ordered(left.bits, right.bits);
    default:
        return 0;
    }
}

} // namespace erlang_aot

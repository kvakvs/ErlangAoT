#include "../../parsing/operator_info.hpp"
#include "../../preprocessor/expression.hpp"
#include "resolver.hpp"
#include <algorithm>
#include <charconv>

namespace erlang_aot::semantic::types {
namespace {
// Decode closed operator identities retained in symbolic syntax nodes.
unsigned operation(const Node &node) {
    unsigned value = 0;
    const auto converted = std::from_chars(node.name.data(), node.name.data() + node.name.size(), value);
    if (converted.ec != std::errc{}) {
        throw std::logic_error("invalid type operator identity");
    }
    return value;
}

// Type singleton text is canonical decimal, so no base-prefix parser or host-width narrowing is needed.
BigInt decimal_value(std::string_view text) {
    const bool negative = text.starts_with('-');
    if (negative) {
        text.remove_prefix(1);
    }
    BigInt value = 0;
    for (const auto digit : text) {
        if (digit < '0' || digit > '9') {
            throw std::logic_error("nondecimal type integer");
        }
        value *= 10;
        value += static_cast<unsigned>(digit - '0');
    }
    return negative ? -value : value;
}

// Exact integer arithmetic shares the already bounded compiler evaluator, never runtime math.
Id arithmetic(const Resolver &r, const Node &node, const std::vector<Value> &values, const ast::NodeSource &source) {
    const auto spelling = node.kind == Kind::unary
                              ? operator_spelling(static_cast<ast::UnaryOperator>(operation(node)))
                              : operator_spelling(static_cast<ast::BinaryOperator>(operation(node)));
    try {
        const auto value = evaluate_operator(spelling, values);
        const auto decimal = decimal_integer(integral(value));
        if (decimal.size() > 10000) {
            throw EvaluationLimit();
        }
        return r.registry.graph.intern({Kind::integer, decimal});
    } catch (const EvaluationLimit &) {
        r.diagnostic(source, "type integer budget exhausted; widened to term()", Severity::warning);
        return r.registry.graph.exhausted();
    } catch (const EvaluationFailure &) {
        r.diagnostic(source, "invalid integer type expression");
        return r.registry.graph.top();
    }
}

// Integer-only syntax categories reject nonconstants instead of treating declarations as executable code.
std::optional<std::vector<Value>> operands(const Resolver &r, const Node &node, const ast::NodeSource &source) {
    std::vector<Value> values;
    for (const auto child : node.children) {
        const auto &value = r.registry.graph.get(child);
        if (value.kind != Kind::integer) {
            if (value.kind != Kind::top || !r.registry.graph.widened()) {
                r.diagnostic(source, "type bound must be an integer constant");
            }
            return {};
        }
        if (value.name.size() > 10000) {
            r.diagnostic(source, "type integer budget exhausted; widened to term()", Severity::warning);
            (void)r.registry.graph.exhausted();
            return {};
        }
        values.push_back(integer(decimal_value(value.name)));
    }
    return values;
}
} // namespace

Id constant_node(Resolver &r, Node node, const ast::NodeSource &source) {
    const auto values = operands(r, node, source);
    if (!values) {
        return r.registry.graph.top();
    }
    if (node.kind == Kind::unary || node.kind == Kind::binary) {
        return arithmetic(r, node, *values, source);
    }
    if (node.kind == Kind::range && (*values)[0].integer >= (*values)[1].integer) {
        r.diagnostic(source, "integer type range must have increasing bounds");
        return r.registry.graph.top();
    }
    if (node.kind == Kind::bitstring &&
        std::ranges::any_of(*values, [](const auto &value) { return value.integer < 0; })) {
        r.diagnostic(source, "bitstring type size must be nonnegative");
        return r.registry.graph.top();
    }
    return r.registry.graph.intern(std::move(node));
}
} // namespace erlang_aot::semantic::types

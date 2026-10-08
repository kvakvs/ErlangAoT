#include "expression_capability.hpp"
#include "funs.hpp"
#include "records.hpp"
#include "services.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
std::string_view ExpressionCapability::operator()(const ast::RecordExpression &value) const {
    const auto *layout = record_layout(module, value.identity);
    const bool native = anonymous_record(value.identity) || external_record(module, value.identity);
    return layout || native ? "" : "heap expressions";
}

std::string_view ExpressionCapability::operator()(const ast::RecordAccess &value) const {
    const auto *layout = record_layout(module, value.identity);
    const bool native = anonymous_record(value.identity) || external_record(module, value.identity);
    return layout || native ? "" : "heap expressions";
}

std::string_view ExpressionCapability::operator()(const ast::RecordIndex &value) const {
    const auto *layout = record_layout(module, value.record, syntax.expression(id).source);
    return layout ? "" : "heap expressions";
}

// try ... of ... catch Class:Reason:Stack ... after runs.
std::string_view ExpressionCapability::operator()(const ast::TryExpression &) const { return {}; }

std::string_view ExpressionCapability::operator()(const ast::IntegerLiteral &) const { return {}; }

std::string_view ExpressionCapability::operator()(const ast::CharacterLiteral &) const {
    return integer_literal(syntax, id, word_bits) ? "" : "bignum expressions";
}

// A fun of an erlang builtin needs a bridge builtin (resolved with services); other guard builtins stay unavailable.
std::string_view ExpressionCapability::operator()(const ast::LocalFunReference &value) const {
    const auto count = arity(value.arity);
    const FunctionKey key{value.name.name, count.value_or(0)};
    const bool builtin = module.fun_entries.contains(&syntax.expression(id));
    return count && !module.lookup.contains(key) && guard_signature(key) && !builtin ? "dynamic calls" : "";
}

// fun M:F/A with variables is built at run time; a literal fun erlang:F/A needs a bridge builtin of that name.
std::string_view ExpressionCapability::operator()(const ast::RemoteFunReference &value) const {
    if (dynamic_fun(value)) {
        const auto *count = std::get_if<Integer>(&value.arity);
        const auto valid = count ? arity(*count) : std::optional<std::size_t>{0};
        return valid && *valid <= 255 ? "" : "dynamic calls";
    }
    const auto names = external_fun(value);
    if (!names) {
        return "dynamic calls";
    }
    const auto &[owner, name, count] = *names;
    return owner != U"erlang" || bridge_builtin({name, count}) ? "" : "dynamic calls";
}
} // namespace erlang_aot::semantic

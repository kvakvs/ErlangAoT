#include "native_records.hpp"
#include "binding_state.hpp"
#include "capabilities.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
namespace {
// A binary default may hold only string segments, as erl_lint's is_literal-binary rule.
bool string_binary(const ast::Module &syntax, const ast::Bitstring &binary) {
    return std::ranges::all_of(binary.segments, [&](const ast::BinarySegment &segment) {
        return !segment.size &&
               std::holds_alternative<ast::StringLiteral>(syntax.expression(ungroup(syntax, segment.value)).value);
    });
}

// Literal scalars, and containers or operators whose operands are literal (OTP folds constant operators).
struct LiteralNode {
    // Binary segments are checked here, since only string segments qualify.
    const ast::Module &syntax;

    bool operator()(const ast::Bitstring &value) const { return string_binary(syntax, value); }

    bool operator()(const ast::MapExpression &value) const { return !value.base; }

    bool operator()(const ast::IntegerLiteral &) const { return true; }

    bool operator()(const ast::CharacterLiteral &) const { return true; }

    bool operator()(const ast::FloatLiteral &) const { return true; }

    bool operator()(const ast::Atom &) const { return true; }

    bool operator()(const ast::StringLiteral &) const { return true; }

    bool operator()(const ast::List &) const { return true; }

    bool operator()(const ast::Tuple &) const { return true; }

    bool operator()(const ast::Group &) const { return true; }

    bool operator()(const ast::UnaryExpression &) const { return true; }

    bool operator()(const ast::BinaryExpression &) const { return true; }

    // Variables, calls, funs, records and every other construct are not literal.
    template <typename Other> bool operator()(const Other &) const { return false; }
};

// Walk a default without recursion; any other node (variable, call, fun, record) makes it illegal.
bool literal(const ast::Module &syntax, const ast::ExprId &root) {
    std::vector<ast::ExprId> pending{root};
    while (!pending.empty()) {
        const auto &value = syntax.expression(pending.back()).value;
        pending.pop_back();
        if (!std::visit(LiteralNode{syntax}, value)) {
            return false;
        }
        if (!std::holds_alternative<ast::Bitstring>(value)) {
            const auto children = binding_children(value);
            pending.insert(pending.end(), children.begin(), children.end());
        }
    }
    return true;
}
} // namespace

void native_defaults(const Module &module, const RecordLayout &layout, const Reporter &out) {
    for (const auto &field : layout.fields) {
        if (field.default_value && !literal(*module.syntax, *field.default_value)) {
            report(module, &module.syntax->expression(*field.default_value).source,
                   "illegal default value for field " + utf8(field.name.name) + " in native record " +
                       utf8(layout.name.name),
                   out);
        }
    }
}

void native_initialized(const Module &module, const ast::RecordExpression &record, const RecordLayout &layout,
                        const std::set<std::u32string> &named, const Reporter &out) {
    for (const auto &field : layout.fields) {
        if (!field.default_value && !named.contains(field.name.name)) {
            report(module, &record.identity.source,
                   "field " + utf8(field.name.name) + " is not initialized in native record " + utf8(layout.name.name),
                   out);
        }
    }
}
} // namespace erlang_aot::semantic

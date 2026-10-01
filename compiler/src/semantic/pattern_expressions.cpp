#include "capabilities.hpp"
#include "pattern_state.hpp"
#include <set>

namespace erlang_aot::semantic {
namespace {
struct ReadExpression {
    // Embedded keys/sizes use guard expression legality, without enabling executable guards.
    BindingAnalysis &state;
    ast::ExprId id;

    template <typename T> bool operator()(const T &) const { return false; }

    bool operator()(const ast::Variable &) const { return true; }

    bool operator()(const ast::IntegerLiteral &) const { return true; }

    bool operator()(const ast::FloatLiteral &) const { return true; }

    bool operator()(const ast::CharacterLiteral &) const { return true; }

    bool operator()(const ast::Atom &) const { return true; }

    bool operator()(const ast::StringLiteral &) const { return true; }

    bool operator()(const ast::Tuple &) const { return true; }

    bool operator()(const ast::List &) const { return true; }

    bool operator()(const ast::Group &) const { return true; }

    bool operator()(const ast::UnaryExpression &) const { return true; }

    bool operator()(const ast::RecordExpression &) const { return true; }

    bool operator()(const ast::RecordAccess &) const { return true; }

    bool operator()(const ast::RecordIndex &) const { return true; }

    bool operator()(const ast::Bitstring &value) const {
        pattern_binary(state, id, value, false);
        return true;
    }

    bool operator()(const ast::CallExpression &value) const { return pattern_call(state, id, value); }

    bool operator()(const ast::BinaryExpression &value) const {
        return value.operation != ast::BinaryOperator::send && value.operation != ast::BinaryOperator::append &&
               value.operation != ast::BinaryOperator::subtract_list;
    }

    bool operator()(const ast::MapExpression &value) const {
        for (const auto &field : value.fields) {
            if (!value.base && field.kind == ast::MapFieldKind::exact) {
                return false;
            }
        }
        return true;
    }
};
} // namespace

std::vector<ast::ExprId> pattern_expression(BindingAnalysis &state, const ast::ExprId &id) {
    const auto &value = state.module.syntax->expression(id).value;
    if (!std::visit(ReadExpression{state, id}, value)) {
        pattern_error(state, id, "illegal expression in pattern key or size");
        return {};
    }
    if (const auto *call = std::get_if<ast::CallExpression>(&value)) {
        return call->arguments;
    }
    return binding_children(value);
}
} // namespace erlang_aot::semantic

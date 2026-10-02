#include "../semantic/bindings.hpp"
#include "../semantic/capabilities.hpp"
#include "../semantic/services.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <algorithm>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Checked canonical decimal parsing rejects arbitrary-size values before LLVM sees them.
llvm::Value *literal(ExpressionLowering &state, const ast::ExprId &expression) {
    const auto &syntax = *state.module.syntax;
    auto *word = state.word;
    const auto value = semantic::integer_literal(syntax, expression, word->getBitWidth());
    if (!value) {
        return lower_integer(state, std::get<ast::IntegerLiteral>(syntax.expression(expression).value).value.decimal);
    }
    const auto encoded = word->getBitWidth() == 32 ? *abi::v1::IntegerEncoding<32>::encode(*value)
                                                   : *abi::v1::IntegerEncoding<64>::encode(*value);
    return llvm::ConstantInt::get(word, encoded);
}

// Resolve one validated read without relying on original-argument projection facts.
llvm::Value *binding(ExpressionLowering &state, const ast::ExprId &expression) {
    for (const auto &binding : state.function.bindings) {
        if (binding.expression == expression && binding.use == semantic::BindingUse::read) {
            return state.bindings.at(binding.identity);
        }
    }
    throw std::invalid_argument("lowering: binding has no available value");
}

// Resolve leaves through existing parameter bindings, preserving every input term unchanged.
llvm::Value *leaf(ExpressionLowering &state, const ast::ExprId &expression) {
    const auto &value = state.module.syntax->expression(expression).value;
    if (std::holds_alternative<ast::Variable>(value)) {
        return binding(state, expression);
    }
    if (const auto *real = std::get_if<ast::FloatLiteral>(&value)) {
        return lower_float(state, real->value);
    }
    if (const auto *atom = std::get_if<ast::Atom>(&value)) {
        return lower_atom(state, *atom);
    }
    if (const auto *map = std::get_if<ast::MapExpression>(&value)) {
        return lower_map(state, *map);
    }
    if (auto *container = lower_container(state, value)) {
        return container;
    }
    return literal(state, expression);
}

// Missing capability authorization is a phase-contract failure, never an unchecked optional access.
abi::v1::ImmediateOperation operation(const std::optional<abi::v1::ImmediateOperation> &value) {
    if (!value) {
        throw std::invalid_argument("lowering: unavailable immediate service");
    }
    return *value;
}

// Keep resolved runtime services and generated calls on their existing checked boundaries.
llvm::Value *call_value(ExpressionLowering &state, const ast::Expression &expression, const ast::CallExpression &call) {
    const auto service = state.function.services.find(&expression);
    if (service != state.function.services.end()) {
        auto *left = state.values.at(&state.module.syntax->expression(call.arguments.at(0)));
        auto *right =
            call.arguments.size() == 2 ? state.values.at(&state.module.syntax->expression(call.arguments[1])) : nullptr;
        return lower_operation(state, operation(service->second.operation), left, right);
    }
    return lower_call(state, expression, call);
}

} // namespace

llvm::Value *lower_value(ExpressionLowering &state, const ast::ExprId &id) {
    const auto &expression = state.module.syntax->expression(id);
    locate_source(state.builder, *state.module.syntax, expression.source);
    if (semantic::integer_literal(*state.module.syntax, id, state.word->getBitWidth())) {
        return literal(state, id);
    }
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        return call_value(state, expression, *call);
    }
    if (const auto *match = std::get_if<ast::MatchExpression>(&expression.value)) {
        return lower_body_match(state, *match);
    }
    if (const auto *binary = std::get_if<ast::BinaryExpression>(&expression.value)) {
        return lower_operation(state, operation(semantic::immediate_operator(binary->operation)),
                               state.values.at(&state.module.syntax->expression(binary->left)),
                               state.values.at(&state.module.syntax->expression(binary->right)));
    }
    if (const auto *group = std::get_if<ast::Group>(&expression.value)) {
        return state.values.at(&state.module.syntax->expression(group->expression));
    }
    if (const auto *unary = std::get_if<ast::UnaryExpression>(&expression.value)) {
        return lower_operation(state, operation(semantic::immediate_unary(unary->operation)),
                               state.values.at(&state.module.syntax->expression(unary->operand)));
    }
    return leaf(state, id);
}

} // namespace erlang_aot::codegen

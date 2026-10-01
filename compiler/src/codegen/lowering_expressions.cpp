#include "../semantic/bindings.hpp"
#include "../semantic/capabilities.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <algorithm>
#include <erlang_aot/abi/term.hpp>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Checked canonical decimal parsing rejects arbitrary-size values before LLVM sees them.
llvm::ConstantInt *literal(const ast::Module &syntax, const ast::ExprId &expression, llvm::IntegerType *word) {
    const auto value = semantic::integer_literal(syntax, expression, word->getBitWidth());
    if (!value) {
        throw std::invalid_argument("lowering: expected a representable integer literal");
    }
    const auto encoded = word->getBitWidth() == 32 ? *abi::v1::IntegerEncoding<32>::encode(*value)
                                                   : *abi::v1::IntegerEncoding<64>::encode(*value);
    return llvm::ConstantInt::get(word, encoded);
}

// Use the binding's original argument index, with target-word alignment and no inbounds promise.
llvm::Value *parameter(llvm::IRBuilder<> &builder, llvm::Function &entry, llvm::IntegerType *word,
                       const std::size_t argument) {
    auto *slot = builder.CreateGEP(word, entry.getArg(1), llvm::ConstantInt::get(word, argument), "argument.slot");
    return builder.CreateAlignedLoad(word, slot, llvm::Align(word->getBitWidth() / 8), "argument");
}

// Resolve leaves through existing parameter bindings, preserving every input term unchanged.
llvm::Value *leaf(ExpressionLowering &state, const ast::ExprId &expression) {
    if (const auto argument = semantic::binding_argument(state.function, expression)) {
        return parameter(state.builder, state.entry, state.word, *argument);
    }
    const auto &value = state.module.syntax->expression(expression).value;
    if (std::holds_alternative<ast::Variable>(value)) {
        throw std::invalid_argument("lowering: binding has no available value");
    }
    if (const auto *atom = std::get_if<ast::Atom>(&value)) {
        return lower_atom(state, *atom);
    }
    return literal(*state.module.syntax, expression, state.word);
}

// Each parent is emitted only after its child values, including grouping and nested calls.
llvm::Value *evaluate(ExpressionLowering &state, const ast::ExprId &id) {
    const auto &expression = state.module.syntax->expression(id);
    locate_source(state.builder, *state.module.syntax, expression.source);
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        return lower_call(state, expression, *call);
    }
    if (const auto *group = std::get_if<ast::Group>(&expression.value)) {
        return state.values.at(&state.module.syntax->expression(group->expression));
    }
    return leaf(state, id);
}

struct Visit {
    // Explicit enter/exit frames keep deeply nested calls off the C++ stack.
    ast::ExprId expression;
    bool ready = false;
};

// Reverse-push children so argument effects and calls remain in Erlang source order.
llvm::Value *body(ExpressionLowering &state, const ast::ExprId &root) {
    std::vector<Visit> pending{{root}};
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        const auto &expression = state.module.syntax->expression(visit.expression);
        if (visit.ready) {
            state.values.emplace(&expression, evaluate(state, visit.expression));
        } else {
            pending.push_back({visit.expression, true});
            const auto children = semantic::expression_children(expression);
            for (auto child = children.rbegin(); child != children.rend(); ++child) {
                pending.push_back({*child});
            }
        }
    }
    return state.values.at(&state.module.syntax->expression(root));
}
} // namespace

llvm::Value *lower_expression(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                              const semantic::Function &function, const ast::ExprId &expression,
                              llvm::IntegerType *word, const semantic::types::Inference &inferred) {
    ExpressionLowering state{builder, entry, module, function, inferred, word, {}};
    return body(state, expression);
}
} // namespace erlang_aot::codegen

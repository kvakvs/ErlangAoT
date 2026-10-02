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
llvm::ConstantInt *literal(const ast::Module &syntax, const ast::ExprId &expression, llvm::IntegerType *word) {
    const auto value = semantic::integer_literal(syntax, expression, word->getBitWidth());
    if (!value) {
        throw std::invalid_argument("lowering: expected a representable integer literal");
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
    if (const auto *atom = std::get_if<ast::Atom>(&value)) {
        return lower_atom(state, *atom);
    }
    if (std::holds_alternative<ast::Tuple>(value)) {
        return llvm::ConstantInt::get(state.word, abi::v1::empty_tuple);
    }
    if (std::holds_alternative<ast::List>(value) || std::holds_alternative<ast::StringLiteral>(value)) {
        return llvm::ConstantInt::get(state.word, abi::v1::empty_list);
    }
    return literal(*state.module.syntax, expression, state.word);
}

// Missing capability authorization is a phase-contract failure, never an unchecked optional access.
abi::v1::ImmediateOperation operation(const std::optional<abi::v1::ImmediateOperation> &value) {
    if (!value) {
        throw std::invalid_argument("lowering: unavailable immediate service");
    }
    return *value;
}

// Each parent is emitted only after its child values, including grouping and nested calls.
llvm::Value *evaluate(ExpressionLowering &state, const ast::ExprId &id) {
    const auto &expression = state.module.syntax->expression(id);
    locate_source(state.builder, *state.module.syntax, expression.source);
    if (const auto *call = std::get_if<ast::CallExpression>(&expression.value)) {
        const auto service = state.function.services.find(&expression);
        if (service != state.function.services.end()) {
            auto *left = state.values.at(&state.module.syntax->expression(call->arguments.at(0)));
            auto *right = call->arguments.size() == 2
                              ? state.values.at(&state.module.syntax->expression(call->arguments[1]))
                              : nullptr;
            return lower_immediate(state, operation(service->second.operation), left, right);
        }
        return lower_call(state, expression, *call);
    }
    if (const auto *binary = std::get_if<ast::BinaryExpression>(&expression.value)) {
        return lower_immediate(state, operation(semantic::immediate_operator(binary->operation)),
                               state.values.at(&state.module.syntax->expression(binary->left)),
                               state.values.at(&state.module.syntax->expression(binary->right)));
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
} // namespace

llvm::Value *lower_body(ExpressionLowering &state, const ast::ExprId &root) {
    std::vector<Visit> pending{{root}};
    while (!pending.empty()) {
        const auto visit = pending.back();
        pending.pop_back();
        const auto &expression = state.module.syntax->expression(visit.expression);
        if (visit.ready) {
            state.values.emplace(&expression, evaluate(state, visit.expression));
        } else {
            pending.push_back({visit.expression, true});
            if (semantic::integer_literal(*state.module.syntax, visit.expression, state.word->getBitWidth())) {
                continue;
            }
            const auto children = semantic::expression_children(expression);
            for (auto child = children.rbegin(); child != children.rend(); ++child) {
                pending.push_back({*child});
            }
        }
    }
    return state.values.at(&state.module.syntax->expression(root));
}

llvm::Value *lower_expression(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                              const semantic::Function &function, const ast::ExprId &expression,
                              llvm::IntegerType *word, const semantic::types::Inference &inferred) {
    ExpressionLowering state{builder, entry, module, function, inferred, word, {}};
    const auto &clause = std::get<ast::Function>(module.syntax->form(function.form).value).clauses.at(0);
    const bool unconditional = lower_unconditional_head(state);
    if (unconditional && !clause.guard) {
        return lower_body(state, expression);
    }
    auto *success = llvm::BasicBlock::Create(entry.getContext(), "match.success", &entry);
    auto *mismatch = llvm::BasicBlock::Create(entry.getContext(), "match.mismatch", &entry);
    auto *guard = clause.guard ? llvm::BasicBlock::Create(entry.getContext(), "guard.entry", &entry) : success;
    if (unconditional) {
        builder.CreateBr(guard);
    } else {
        lower_head(state, guard, mismatch);
    }
    builder.SetInsertPoint(mismatch);
    raise_function_clause(state);
    if (clause.guard) {
        builder.SetInsertPoint(guard);
        state.rejection = mismatch;
        auto *test = lower_body(state, clause.guard->alternatives.at(0).tests.at(0));
        auto *truth = lower_atom(state, ast::Atom{U"true"});
        builder.CreateCondBr(lower_exact(state, test, truth), success, mismatch);
        state.rejection = nullptr;
    }
    builder.SetInsertPoint(success);
    return lower_body(state, expression);
}
} // namespace erlang_aot::codegen

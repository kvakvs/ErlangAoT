#include "../semantic/capabilities.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"

namespace erlang_aot::codegen {
namespace {
// Index validated reads once without transferring bindings between candidate environments.
BindingReads read_bindings(const semantic::Module &module, const semantic::Function &function) {
    BindingReads result;
    for (const auto &binding : function.bindings) {
        if (binding.use == semantic::BindingUse::read) {
            result.emplace(&module.syntax->expression(binding.expression), binding.identity);
        }
    }
    return result;
}

// Follow one tail position: blocks and groups end in their last expression, case and if in each clause's.
void tail_position(const ExpressionLowering &state, const ast::Expression &expression,
                   std::vector<ast::ExprId> &pending, std::set<const ast::Expression *> &calls) {
    if (const auto *group = std::get_if<ast::Group>(&expression.value)) {
        pending.push_back(group->expression);
    } else if (const auto *block = std::get_if<ast::BlockExpression>(&expression.value)) {
        pending.push_back(block->body.back());
    } else if (std::holds_alternative<ast::CaseExpression>(expression.value) ||
               std::holds_alternative<ast::IfExpression>(expression.value)) {
        for (const auto &clause : semantic::branch_clauses(expression.value)) {
            pending.push_back(clause.body->back());
        }
    } else if (std::holds_alternative<ast::CallExpression>(expression.value) &&
               !state.function.services.contains(&expression) && state.inferred.callees.contains(&expression)) {
        calls.insert(&expression);
    }
}

// Calls in tail position of any clause, outside every catch and try; their callee's result is the function's.
std::set<const ast::Expression *> tail_calls(const ExpressionLowering &state,
                                             const std::vector<ast::FunctionClause> &clauses) {
    std::set<const ast::Expression *> calls;
    std::vector<ast::ExprId> pending;
    pending.reserve(clauses.size());
    for (const auto &clause : clauses) {
        pending.push_back(clause.body.back());
    }
    while (!pending.empty()) {
        const auto &expression = state.module.syntax->expression(pending.back());
        pending.pop_back();
        tail_position(state, expression, pending, calls);
    }
    return calls;
}

// Each candidate owns fresh SSA maps; its failed head/guard can only enter the next candidate.
void candidate(ExpressionLowering &state, const ast::FunctionClause &clause, llvm::BasicBlock *mismatch) {
    const bool unconditional = lower_unconditional_head(state);
    if (!unconditional || clause.guard) {
        auto *success = llvm::BasicBlock::Create(state.entry.getContext(), "match.success", &state.entry);
        auto *guard =
            clause.guard ? llvm::BasicBlock::Create(state.entry.getContext(), "guard.entry", &state.entry) : success;
        if (unconditional) {
            state.builder.CreateBr(guard);
        } else {
            lower_head(state, guard, mismatch);
        }
        if (clause.guard) {
            state.builder.SetInsertPoint(guard);
            lower_guard(state, *clause.guard, {.success = success, .rejection = mismatch});
        }
        state.builder.SetInsertPoint(success);
    }
    llvm::Value *result = nullptr;
    for (const auto &expression : clause.body) {
        result = lower_body(state, expression);
    }
    locate_source(state.builder, *state.module.syntax, state.module.syntax->expression(clause.body.back()).source);
    state.builder.CreateRet(result);
}
} // namespace

void lower_function(llvm::IRBuilder<> &builder, llvm::Function &entry, const semantic::Module &module,
                    const semantic::Function &function, llvm::IntegerType *word,
                    const semantic::types::Inference &inferred) {
    const auto &clauses = std::get<ast::Function>(module.syntax->form(function.form).value).clauses;
    const auto reads = read_bindings(module, function);
    ExpressionLowering initial{builder, entry, module, function, inferred, word, {}};
    initial.reads = &reads;
    const auto tails = tail_calls(initial, clauses);
    auto roots = begin_roots(initial);
    initial.roots = &roots;
    root_arguments(initial);
    auto *exhausted = llvm::BasicBlock::Create(entry.getContext(), "match.mismatch", &entry);
    llvm::BasicBlock *failure = initial.failure;
    for (std::size_t index = 0; index < clauses.size(); ++index) {
        auto *next = index + 1 == clauses.size() ? exhausted
                                                 : llvm::BasicBlock::Create(entry.getContext(), "clause.next", &entry);
        ExpressionLowering state{builder, entry, module, function, inferred, word, {}, &reads, index};
        state.roots = &roots;
        state.tail_calls = &tails;
        state.failure = failure;
        reset_candidate_roots(state);
        candidate(state, clauses[index], next);
        failure = state.failure;
        builder.SetInsertPoint(next);
    }
    if (exhausted->use_empty()) {
        builder.ClearInsertionPoint();
        exhausted->eraseFromParent();
    } else {
        raise_function_clause(initial);
    }
    finish_roots(initial);
}
} // namespace erlang_aot::codegen

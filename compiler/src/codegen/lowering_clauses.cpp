#include "lowering_state.hpp"
#include "source_locations.hpp"

namespace erlang_aot::codegen {
namespace {
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
    ExpressionLowering initial{builder, entry, module, function, inferred, word, {}};
    auto roots = begin_roots(initial);
    initial.roots = &roots;
    root_arguments(initial);
    auto *exhausted = llvm::BasicBlock::Create(entry.getContext(), "match.mismatch", &entry);
    llvm::BasicBlock *failure = initial.failure;
    for (std::size_t index = 0; index < clauses.size(); ++index) {
        auto *next = index + 1 == clauses.size() ? exhausted
                                                 : llvm::BasicBlock::Create(entry.getContext(), "clause.next", &entry);
        ExpressionLowering state{builder, entry, module, function, inferred, word, {}, index};
        state.roots = &roots;
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

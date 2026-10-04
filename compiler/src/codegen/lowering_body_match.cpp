#include "lowering_state.hpp"
#include <array>
#include <stdexcept>

namespace erlang_aot::codegen {
semantic::MatchPlan body_pattern_plan(const ExpressionLowering &state, const ast::ExprId &pattern) {
    auto plan =
        semantic::make_match_plan(state.module, state.function, pattern,
                                  [](const Diagnostic &diagnostic) { throw std::invalid_argument(render(diagnostic)); },
                                  {.clause = state.clause, .word_bits = state.word->getBitWidth()});
    if (!plan) {
        throw std::invalid_argument("lowering: unavailable body match plan");
    }
    return std::move(*plan);
}

llvm::Value *lower_body_match(ExpressionLowering &state, const ast::MatchExpression &match) {
    auto *value = state.values.at(&state.module.syntax->expression(match.right));
    const auto plan = body_pattern_plan(state, match.left);
    auto *success = llvm::BasicBlock::Create(state.entry.getContext(), "body.match.success", &state.entry);
    auto *mismatch = llvm::BasicBlock::Create(state.entry.getContext(), "body.match.failure", &state.entry);
    lower_match_plan(state, plan, std::array{value}, success, mismatch);
    if (mismatch->use_empty()) {
        mismatch->eraseFromParent();
    } else {
        state.builder.SetInsertPoint(mismatch);
        raise_reason(state, abi::v1::ErrorReason::badmatch, value);
    }
    state.builder.SetInsertPoint(success);
    return value;
}
} // namespace erlang_aot::codegen

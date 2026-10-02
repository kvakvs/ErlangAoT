#include "lowering_state.hpp"

namespace erlang_aot::codegen {
namespace {
// Comma boundaries require canonical true and share one rejection edge for the complete alternative.
void conjunction(ExpressionLowering &state, const ast::GuardConjunction &guard, GuardEdges edges) {
    for (std::size_t i = 0; i < guard.tests.size(); ++i) {
        auto *next = i + 1 == guard.tests.size()
                         ? edges.success
                         : llvm::BasicBlock::Create(state.entry.getContext(), "guard.next", &state.entry);
        auto *value = lower_body(state, guard.tests[i]);
        auto *truth = lower_atom(state, ast::Atom{U"true"});
        state.builder.CreateCondBr(lower_exact(state, value, truth), next, edges.rejection);
        if (next != edges.success) {
            state.builder.SetInsertPoint(next);
        }
    }
}
} // namespace

void lower_guard(ExpressionLowering &state, const ast::GuardSyntax &guard, GuardEdges edges) {
    for (std::size_t i = 0; i < guard.alternatives.size(); ++i) {
        auto *reject = i + 1 == guard.alternatives.size()
                           ? edges.rejection
                           : llvm::BasicBlock::Create(state.entry.getContext(), "guard.alternative", &state.entry);
        state.rejection = reject;
        conjunction(state, guard.alternatives[i], {.success = edges.success, .rejection = reject});
        if (reject != edges.rejection) {
            state.builder.SetInsertPoint(reject);
        }
    }
    state.rejection = nullptr;
}
} // namespace erlang_aot::codegen

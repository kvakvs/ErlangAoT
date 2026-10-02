#pragma once
#include <llvm/IR/IRBuilder.h>

namespace erlang_aot::codegen {
struct ExpressionLowering;

struct FunctionRoots {
    // Patch the runtime buffer extent after all emitted candidate values have assigned their slots.
    llvm::CallInst *buffer;
    llvm::IntegerType *word;
    // Arguments persist across candidates; temporary slots are reused after candidate rejection.
    std::size_t arguments;
    std::size_t next = 0;
    std::size_t capacity = 0;
};

// Allocate a checked runtime root scope before any source body or heap allocation can execute.
FunctionRoots begin_roots(ExpressionLowering &state);
// Retain original arguments before attempting any candidate.
void root_arguments(ExpressionLowering &state);
// Publish a produced term after its fallible operation succeeds and before another allocation.
void root_value(ExpressionLowering &state, llvm::Value *value);
// Drop rejected-candidate temporaries while retaining the original argument roots.
void reset_candidate_roots(ExpressionLowering &state);
// Fix the maximum buffer extent and transfer return/error ownership at every generated exit.
void finish_roots(ExpressionLowering &state);
} // namespace erlang_aot::codegen

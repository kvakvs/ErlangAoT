#pragma once
#include <llvm/IR/IRBuilder.h>
#include <string_view>

namespace erlang_aot::codegen {
struct ExpressionLowering;

// Placeholder for a native-form function's frame slots: (context, slot count, name descriptor) -> slots.
// lower_frames replaces it; it never reaches emitted objects.
inline constexpr std::string_view FRAME_MARKER = "erlang_aot.frame";
// Marks native-form Erlang function definitions and declarations with their arity.
inline constexpr std::string_view ARITY_ATTRIBUTE = "erlang-arity";

struct FunctionRoots {
    // Patch the frame's term slot count after all emitted candidate values have assigned their slots.
    llvm::CallInst *buffer;
    llvm::IntegerType *word;
    // Arguments persist across candidates; temporary slots are reused after candidate rejection.
    std::size_t arguments;
    std::size_t next = 0;
    std::size_t capacity = 0;
};

// Name the frame slots before any source body or heap allocation can execute.
FunctionRoots begin_roots(ExpressionLowering &state);
// Retain original arguments before attempting any candidate.
void root_arguments(ExpressionLowering &state);
// Publish a produced term after its fallible operation succeeds and before another allocation.
void root_value(ExpressionLowering &state, llvm::Value *value);
// Reserve a zero-initialized runtime slot for a service's success-only output or constructor argument.
llvm::Value *root_slot(ExpressionLowering &state);
// Drop rejected-candidate temporaries while retaining the original argument roots.
void reset_candidate_roots(ExpressionLowering &state);
// Fix the frame's term slot count once every candidate has been lowered.
void finish_roots(ExpressionLowering &state);
} // namespace erlang_aot::codegen

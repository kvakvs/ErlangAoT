#pragma once

namespace llvm {
class FunctionType;
class IntegerType;
} // namespace llvm

namespace erlang_aot::codegen {
class Compilation;

// Derive the unsigned term's LLVM integer type from a configured target, diagnosing unsupported layouts.
llvm::IntegerType *term_type(Compilation &compilation);
// Build the private native-convention signature: term(context pointer, borrowed argument-array pointer).
llvm::FunctionType *generated_function_type(Compilation &compilation);
} // namespace erlang_aot::codegen

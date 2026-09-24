#pragma once
#include "compilation.hpp"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <memory>
#include <vector>

namespace erlang_aot::codegen::detail {
// Backend-only storage; field order is part of the module/context/callback lifetime contract.
struct CompilationState {
    // Initialize a separate context and one empty IR module for each owned syntax input.
    explicit CompilationState(CompilationRequest request);
    // Retain the batch and source provenance until all LLVM state has been released.
    CompilationRequest request;
    // Outlive the context whose diagnostic callback borrows this result's stable address.
    CompilationResult result;
    // Isolate LLVM types, constants and diagnostics between compilation instances.
    std::unique_ptr<llvm::LLVMContext> context;
    // Retain target layout/code-generation policy until modules are destroyed.
    std::unique_ptr<llvm::TargetMachine> target_machine;
    // Destroy all modules before their shared context; indices match request input order.
    std::vector<std::unique_ptr<llvm::Module>> modules;
};

// Copy LLVM diagnostics into the result; catch callback failures before returning to LLVM.
void capture_diagnostic(const llvm::DiagnosticInfo *diagnostic, void *destination) noexcept;
// Register only CMake-selected SDK backends once, safely across compilation instances.
void initialize_target_backends();
} // namespace erlang_aot::codegen::detail

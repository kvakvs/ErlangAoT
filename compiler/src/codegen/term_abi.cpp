#include "term_abi.hpp"
#include "llvm_state.hpp"
#include <llvm/IR/DerivedTypes.h>

namespace erlang_aot::codegen {
llvm::IntegerType *term_type(Compilation &compilation) {
    auto &state = detail::state(compilation);
    if (state.result.status() != CompilationStatus::incomplete) {
        return nullptr;
    }
    if (!state.target_machine) {
        state.result.report({DiagnosticLevel::error, "Term ABI requires a configured target machine", {}, {}});
        return nullptr;
    }
    const auto layout = state.target_machine->createDataLayout();
    const auto bits = layout.getPointerSizeInBits();
    if (bits != 32 && bits != 64) {
        state.result.report({DiagnosticLevel::error, "Term ABI requires 32-bit or 64-bit target words", {}, {}});
        return nullptr;
    }
    auto *type = llvm::IntegerType::get(*state.context, bits);
    if (layout.getABITypeAlign(type).value() != bits / 8 || layout.getPointerABIAlignment(0).value() != bits / 8 ||
        layout.isNonIntegralAddressSpace(0)) {
        state.result.report(
            {DiagnosticLevel::error, "Term ABI requires integral word-aligned pointers and terms", {}, {}});
        return nullptr;
    }
    return type;
}

llvm::FunctionType *generated_function_type(Compilation &compilation) {
    auto *word = term_type(compilation);
    if (word == nullptr) {
        return nullptr;
    }
    auto *pointer = llvm::PointerType::get(*detail::state(compilation).context, 0);
    return llvm::FunctionType::get(word, {pointer, pointer}, false);
}
} // namespace erlang_aot::codegen

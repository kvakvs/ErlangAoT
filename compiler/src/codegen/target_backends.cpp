#include "llvm_state.hpp"
#include <llvm/Support/TargetSelect.h>
#include <mutex>

namespace clause::codegen::detail {
namespace {
// Keep registry initialization matched to SDK availability, including component-library builds.
void register_backends() {
#ifdef CLAUSE_LLVM_X86
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
    LLVMInitializeX86AsmPrinter();
    LLVMInitializeX86AsmParser();
#endif
#ifdef CLAUSE_LLVM_ARM
    LLVMInitializeARMTargetInfo();
    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
    LLVMInitializeARMAsmPrinter();
    LLVMInitializeARMAsmParser();
#endif
#ifdef CLAUSE_LLVM_AArch64
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmPrinter();
    LLVMInitializeAArch64AsmParser();
#endif
}
} // namespace

void initialize_target_backends() {
    static std::once_flag initialized;
    std::call_once(initialized, register_backends);
}
} // namespace clause::codegen::detail

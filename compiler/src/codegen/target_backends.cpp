#include "llvm_state.hpp"
#include <llvm/Support/TargetSelect.h>
#include <mutex>

namespace erlang_aot::codegen::detail {
namespace {
// Keep registry initialization matched to SDK availability, including component-library builds.
void register_backends() {
#ifdef ERLANG_AOT_LLVM_X86
    LLVMInitializeX86TargetInfo();
    LLVMInitializeX86Target();
    LLVMInitializeX86TargetMC();
#endif
#ifdef ERLANG_AOT_LLVM_ARM
    LLVMInitializeARMTargetInfo();
    LLVMInitializeARMTarget();
    LLVMInitializeARMTargetMC();
#endif
#ifdef ERLANG_AOT_LLVM_AArch64
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
#endif
}
} // namespace

void initialize_target_backends() {
    static std::once_flag initialized;
    std::call_once(initialized, register_backends);
}
} // namespace erlang_aot::codegen::detail

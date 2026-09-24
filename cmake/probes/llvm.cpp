#include <llvm/Config/llvm-config.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <string>

#if !defined(__cpp_exceptions) || !defined(__cpp_rtti)
#error "ErlangAoT requires C++ exceptions and RTTI in its host toolchain"
#endif

#if LLVM_VERSION_MAJOR != 23 || LLVM_VERSION_MINOR != 1 || LLVM_VERSION_PATCH < 1
#error "LLVM headers must match the supported 23.1.x release line"
#endif

// Exercise the C++ SDK ABI without initializing targets or emitting code.
int main() {
    llvm::LLVMContext context;
    const llvm::Module module(std::string("sdk_probe"), context);
    return module.getName() == "sdk_probe" ? 0 : 1;
}

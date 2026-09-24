#include "codegen/sdk.hpp"
#include <iostream>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

// Prove both the private backend and the installed C++ SDK link and execute together.
int main() {
    llvm::LLVMContext context;
    const llvm::Module module("sdk_smoke", context);
    if (module.getName() != "sdk_smoke" || !erlang_aot::codegen::sdk_version().starts_with("23.1.")) {
        std::cerr << "LLVM SDK smoke check failed\n";
        return 1;
    }
    return 0;
}

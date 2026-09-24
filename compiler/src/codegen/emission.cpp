#include "emission.hpp"
#include "llvm_state.hpp"
#include "verification.hpp"
#include <cstring>
#include <exception>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

namespace erlang_aot::codegen {
namespace {
// Retain module context and discard the entire batch if code generation cannot produce an object.
bool reject(detail::CompilationState &state, const llvm::Module &module, const std::string &reason) {
    state.result.report({DiagnosticLevel::error,
                         "LLVM object emission failed for module '" + module.getModuleIdentifier() + "': " + reason,
                         {},
                         {}});
    return false;
}

// Run LLVM's machine-code pipeline on a clone so repeated emission preserves the original IR.
bool emit_module(detail::CompilationState &state, const llvm::Module &module) {
    auto working = llvm::CloneModule(module);
    llvm::SmallVector<char, 0> bytes;
    llvm::raw_svector_ostream stream(bytes);
    llvm::legacy::PassManager passes;
    if (state.target_machine->addPassesToEmitFile(passes, stream, nullptr, llvm::CodeGenFileType::ObjectFile, false)) {
        return reject(state, module, "target does not support object emission");
    }
    passes.run(*working);
    if (state.result.status() == CompilationStatus::failed) {
        return false;
    }
    if (bytes.empty()) {
        return reject(state, module, "target produced an empty object");
    }
    OutputBuffer output{module.getModuleIdentifier(), OutputKind::object, std::vector<std::byte>(bytes.size())};
    std::memcpy(output.bytes.data(), bytes.data(), bytes.size());
    return state.result.add_output(std::move(output));
}
} // namespace

bool emit_objects(Compilation &compilation) {
    if (!verify_ir(compilation)) {
        return false;
    }
    auto &state = detail::state(compilation);
    state.result.discard_outputs();
    for (const auto &module : state.modules) {
        try {
            if (!emit_module(state, *module)) {
                return false;
            }
        } catch (const std::exception &error) {
            return reject(state, *module, error.what());
        }
    }
    return true;
}
} // namespace erlang_aot::codegen

#include "emission.hpp"
#include "bounded_stream.hpp"
#include "limits.hpp"
#include "llvm_state.hpp"
#include "progress.hpp"
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
    state.result.report(
        {.level = DiagnosticLevel::error,
         .message = "LLVM object emission failed for module '" + module.getModuleIdentifier() + "': " + reason,
         .location = {},
         .module_name = {}});
    return false;
}

// Run LLVM's machine-code pipeline on a clone so repeated emission preserves the original IR.
bool emit_module(detail::CompilationState &state, const llvm::Module &module, bool startup) {
    const auto working = llvm::CloneModule(module);
    BoundedStream stream(output_capacity(state.request.limits, state.result.outputs()));
    llvm::legacy::PassManager passes;
    if (state.target_machine->addPassesToEmitFile(passes, stream, nullptr, llvm::CodeGenFileType::ObjectFile, false)) {
        return reject(state, module, "target does not support object emission");
    }
    passes.run(*working);
    if (state.result.status() == CompilationStatus::failed) {
        return false;
    }
    auto bytes = stream.take_bytes();
    if (bytes.empty()) {
        return reject(state, module, "target produced an empty object");
    }
    OutputBuffer output{.module_name = module.getModuleIdentifier(),
                        .kind = OutputKind::object,
                        .bytes = std::move(bytes),
                        .startup = startup};
    return state.result.add_output(std::move(output));
}
} // namespace

bool emit_objects(Compilation &compilation) {
    if (!verify_ir(compilation)) {
        return false;
    }
    auto &state = detail::state(compilation);
    state.result.discard_outputs();
    std::size_t index = 0;
    for (const auto &module : state.modules) {
        // The startup module, when present, follows the batch's inputs.
        const bool startup = index >= state.request.inputs.size();
        progress_module(compilation, index++, "emission");
        try {
            if (!emit_module(state, *module, startup)) {
                return false;
            }
        } catch (const std::exception &error) {
            return reject(state, *module, error.what());
        }
    }
    return true;
}
} // namespace erlang_aot::codegen

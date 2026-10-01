#include "serialization.hpp"
#include "bounded_stream.hpp"
#include "limits.hpp"
#include "llvm_state.hpp"
#include "progress.hpp"
#include "source_annotations.hpp"
#include "verification.hpp"
#include <cstring>
#include <exception>
#include <llvm/Bitcode/BitcodeWriter.h>
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Use LLVM's own writers for both serialized formats, retaining owned bytes after teardown.
OutputBuffer serialize(const llvm::Module &module, const ast::Module &syntax, const SourceScopes &sources,
                       OutputKind kind, std::size_t capacity) {
    BoundedStream stream(capacity);
    if (kind == OutputKind::llvm_ir) {
        SourceAnnotations annotations(module, syntax, sources, capacity);
        annotations.print_sources(stream);
        module.print(stream, &annotations);
    } else {
        llvm::WriteBitcodeToFile(module, stream);
    }
    return {.module_name = module.getModuleIdentifier(), .kind = kind, .bytes = stream.take_bytes()};
}

// Verify all modules before capturing any snapshot; failures discard earlier staged artifacts.
std::optional<std::vector<OutputBuffer>> capture(Compilation &compilation, OutputKind kind, bool artifact) {
    if (!verify_ir(compilation)) {
        return {};
    }
    std::vector<OutputBuffer> outputs;
    try {
        std::size_t index = 0;
        for (const auto &module : detail::state(compilation).modules) {
            if (artifact) {
                progress_module(compilation, index, "emission");
            }
            outputs.push_back(serialize(*module, compilation.request().inputs.at(index).syntax,
                                        detail::state(compilation).source_scopes, kind,
                                        output_capacity(compilation.request().limits, outputs)));
            ++index;
        }
    } catch (const std::exception &error) {
        compilation.result().report({.level = DiagnosticLevel::error,
                                     .message = "LLVM serialization failed: " + std::string(error.what()),
                                     .location = {},
                                     .module_name = {}});
        return {};
    }
    return outputs;
}
} // namespace

std::optional<std::vector<OutputBuffer>> snapshot_ir(Compilation &compilation) {
    return capture(compilation, OutputKind::llvm_ir, false);
}

bool emit_ir(Compilation &compilation, OutputKind kind) {
    if (kind == OutputKind::object) {
        throw std::invalid_argument("IR serialization requires text or bitcode output");
    }
    auto outputs = capture(compilation, kind, true);
    if (!outputs) {
        return false;
    }
    compilation.result().discard_outputs();
    for (auto &output : *outputs) {
        if (!compilation.result().add_output(std::move(output))) {
            return false;
        }
    }
    return true;
}
} // namespace erlang_aot::codegen

#include "inspection.hpp"
#include "../codegen/optimization.hpp"
#include "../codegen/serialization.hpp"
#include "../project/paths.hpp"
#include "display.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace erlang_aot::cli {
namespace {
// Capture only requested stages so optimized-only inspection never retains an unnecessary pre-pipeline copy.
bool capture(codegen::Compilation &compilation, bool requested, std::vector<codegen::OutputBuffer> &outputs) {
    if (!requested) {
        return true;
    }
    auto snapshot = codegen::snapshot_ir(compilation);
    if (!snapshot) {
        return false;
    }
    outputs = std::move(*snapshot);
    return true;
}

// Separate concatenated modules with safe LLVM comments while leaving single snapshots plain assembly.
void print_snapshot(const codegen::OutputBuffer &output, const std::filesystem::path &source, std::string_view target,
                    std::string_view stage, bool headers) {
    if (headers) {
        std::cout << "; erlangaot target=" << quote_text(target) << " module=" << quote_text(output.module_name)
                  << " source=" << quote_text(project::path_text(source)) << " stage=" << stage << '\n';
    }
    if (output.bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
        throw std::runtime_error("IR snapshot exceeds supported stream size");
    }
    std::cout.write(reinterpret_cast<const char *>(output.bytes.data()),
                    static_cast<std::streamsize>(output.bytes.size()));
    std::cout << '\n';
    if (!std::cout) {
        throw std::runtime_error("cannot write IR inspection output");
    }
}

// Preserve source order with before/after adjacent for each module, including separate project targets.
void print_snapshots(const codegen::Compilation &compilation, const FrontendRequest &request,
                     const std::vector<codegen::OutputBuffer> &before,
                     const std::vector<codegen::OutputBuffer> &after) {
    const auto &inputs = compilation.request().inputs;
    const bool headers = request.multiple_targets || inputs.size() > 1 || (!before.empty() && !after.empty());
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (!before.empty()) {
            print_snapshot(before.at(i), inputs[i].source_path, request.project_target, "before", headers);
        }
        if (!after.empty()) {
            print_snapshot(after.at(i), inputs[i].source_path, request.project_target, "after", headers);
        }
    }
}
} // namespace

bool inspect_ir(codegen::Compilation &compilation, const FrontendRequest &request) {
    std::vector<codegen::OutputBuffer> before;
    std::vector<codegen::OutputBuffer> after;
    if (!capture(compilation, request.backend.print_ir, before)) {
        return false;
    }
    if (request.backend.print_optimized_ir && !codegen::optimize(compilation)) {
        return false;
    }
    if (!capture(compilation, request.backend.print_optimized_ir, after)) {
        return false;
    }
    print_snapshots(compilation, request, before, after);
    return true;
}
} // namespace erlang_aot::cli

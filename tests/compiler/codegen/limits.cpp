#include "codegen/limits.hpp"
#include "codegen/serialization.hpp"
#include "lowering_support.hpp"

// Inject small ceilings through the real fixture pipeline without allocating enormous hostile inputs.
void syntax_limits() {
    auto compilation = fixtures({"fact_budget.erl", "answer.erl", "client.erl"});
    for (const auto limits : {cg::CompilationLimits{.modules = 1}, cg::CompilationLimits{.module_nodes = 1},
                              cg::CompilationLimits{.batch_nodes = 1}}) {
        bool rejected = false;
        try {
            cg::validate_input_limits(compilation.request().inputs, limits);
        } catch (const std::length_error &error) {
            rejected = std::string_view(error.what()).contains("limit exceeded");
        }
        require(rejected, "syntax budget did not reject real source");
    }
}

// Byte ceilings must stop LLVM writes and invalidate earlier outputs for all three artifact kinds.
void artifact_limits(cg::OutputKind kind, bool batch) {
    auto compilation = fixtures({"fact_budget.erl", "answer.erl", "client.erl"});
    require(analyze_and_lower(compilation), "limit fixture lowering failed");
    auto &limits = cg::detail::state(compilation).request.limits;
    auto emit = [&] {
        return kind == cg::OutputKind::object ? cg::emit_objects(compilation) : cg::emit_ir(compilation, kind);
    };
    require(emit(), "baseline emission failed");
    const auto first_size = compilation.result().outputs().front().bytes.size();
    if (batch) {
        limits.batch_bytes = first_size;
    } else {
        limits.module_bytes = first_size - 1;
    }
    require(!emit(), "artifact limit was ignored");
    require(compilation.result().outputs().empty() && compilation.result().status() == cg::CompilationStatus::failed,
            "limit rejection retained staged artifacts");
    require(compilation.result().diagnostics().back().message.contains("byte limit"), "wrong limit diagnostic");
}

// Inspection uses the same writer ceiling and cannot retain earlier successful snapshots after failure.
void snapshot_limit(bool tiny) {
    auto compilation = fixture("fact_budget.erl");
    cg::detail::state(compilation).request.annotate_source = true;
    require(analyze_and_lower(compilation), "snapshot fixture lowering failed");
    const auto baseline = cg::snapshot_ir(compilation);
    require(baseline && !baseline->front().bytes.empty(), "annotated snapshot failed");
    cg::detail::state(compilation).request.limits.module_bytes = tiny ? 1 : baseline->front().bytes.size() - 1;
    require(!cg::snapshot_ir(compilation), "snapshot limit ignored");
    require(compilation.result().status() == cg::CompilationStatus::failed, "snapshot failure not latched");
}

// Exercise deterministic unreachable ceilings through genuine parsing, lowering, writers and cleanup.
int main() {
    try {
        syntax_limits();
        for (const auto kind : {cg::OutputKind::object, cg::OutputKind::llvm_ir, cg::OutputKind::llvm_bitcode}) {
            artifact_limits(kind, false);
            artifact_limits(kind, true);
        }
        snapshot_limit(false);
        snapshot_limit(true);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

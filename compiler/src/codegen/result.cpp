#include "result.hpp"
#include <utility>

namespace erlang_aot::codegen {
CompilationStatus CompilationResult::status() const { return status_; }

std::span<const CompilationDiagnostic> CompilationResult::diagnostics() const { return diagnostics_; }

std::span<const OutputBuffer> CompilationResult::outputs() const { return outputs_; }

bool CompilationResult::diagnostic_capture_failed() const { return diagnostic_capture_failed_; }

void CompilationResult::report(CompilationDiagnostic diagnostic) {
    if (diagnostic.level == DiagnosticLevel::error) {
        status_ = CompilationStatus::failed;
        outputs_.clear();
    }
    diagnostics_.push_back(std::move(diagnostic));
}

bool CompilationResult::add_output(OutputBuffer output) {
    if (status_ != CompilationStatus::incomplete) {
        return false;
    }
    outputs_.push_back(std::move(output));
    return true;
}

void CompilationResult::discard_outputs() { outputs_.clear(); }

bool CompilationResult::complete() {
    if (status_ == CompilationStatus::failed) {
        return false;
    }
    status_ = CompilationStatus::succeeded;
    return true;
}

void CompilationResult::fail_diagnostic_capture() noexcept {
    status_ = CompilationStatus::failed;
    diagnostic_capture_failed_ = true;
    outputs_.clear();
}
} // namespace erlang_aot::codegen

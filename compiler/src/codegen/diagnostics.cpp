#include "llvm_state.hpp"
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <llvm/Support/raw_ostream.h>

namespace erlang_aot::codegen::detail {
namespace {
// Preserve all LLVM severity categories without treating informational remarks as errors.
DiagnosticLevel level(const llvm::DiagnosticSeverity severity) {
    switch (severity) {
    case llvm::DS_Error:
        return DiagnosticLevel::error;
    case llvm::DS_Warning:
        return DiagnosticLevel::warning;
    case llvm::DS_Remark:
    case llvm::DS_Note:
        return DiagnosticLevel::note;
    }
    return DiagnosticLevel::error;
}

// Finish formatting before transferring the string away from its stream.
std::string message(const llvm::DiagnosticInfo &diagnostic) {
    std::string text;
    {
        llvm::raw_string_ostream stream(text);
        llvm::DiagnosticPrinterRawOStream printer(stream);
        diagnostic.print(printer);
    }
    return text;
}
} // namespace

void capture_diagnostic(const llvm::DiagnosticInfo *diagnostic, void *destination) noexcept {
    auto &result = *static_cast<CompilationResult *>(destination);
    try {
        result.report({.level = level(diagnostic->getSeverity()),
                       .message = message(*diagnostic),
                       .location = std::nullopt,
                       .module_name = {}});
    } catch (...) {
        result.fail_diagnostic_capture();
    }
}
} // namespace erlang_aot::codegen::detail

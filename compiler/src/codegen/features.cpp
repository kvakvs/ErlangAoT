#include "features.hpp"
#include <utility>

namespace erlang_aot::codegen {
namespace {
// Copy borrowed context into the existing owned diagnostic model before the reporter returns.
CompilationDiagnostic diagnostic(const abi::v1::FeatureId feature, const abi::v1::FeatureContext &context) {
    CompilationDiagnostic diagnostic{.level = DiagnosticLevel::error,
                                     .message = abi::v1::format_feature_failure(feature, context),
                                     .location = {},
                                     .module_name = std::string(context.module)};
    if (!context.source.empty()) {
        diagnostic.location =
            LogicalLocation{.file = std::string(context.source), .line = context.line, .column = context.column};
    }
    diagnostic.reported = true;
    return diagnostic;
}
} // namespace

bool reject_feature(CompilationResult &result, const abi::v1::FeatureId feature, const abi::v1::FeatureContext &context,
                    std::ostream &errors) noexcept {
    if (result.status() != CompilationStatus::incomplete) {
        return false;
    }
    try {
        auto report = diagnostic(feature, context);
        const auto message = report.message;
        result.report(std::move(report));
        errors << message << '\n';
        if (!errors) {
            result.fail_diagnostic_capture();
        }
    } catch (...) {
        result.fail_diagnostic_capture();
    }
    return false;
}
} // namespace erlang_aot::codegen

#include <cstdio>
#include <erlang_aot/runtime/features.hpp>

namespace erlang_aot::runtime {
namespace {
// Default runtime diagnostics use stderr without consulting verbosity or contaminating program stdout.
bool write_stderr(std::string_view message) {
    std::string line(message);
    line += '\n';
    return std::fwrite(line.data(), 1, line.size(), stderr) == line.size();
}
} // namespace

FeatureFailure::FeatureFailure(DiagnosticSink sink) noexcept : sink_(sink) {}

eaot_v1_status FeatureFailure::status() const noexcept { return status_; }

eaot_v1_status FeatureFailure::report(abi::v1::FeatureId feature, const abi::v1::FeatureContext &context) noexcept {
    if (status_ != EAOT_V1_STATUS_OK) {
        return status_;
    }
    status_ =
        abi::v1::find_feature(feature) == nullptr ? EAOT_V1_STATUS_INVALID_ARGUMENT : EAOT_V1_STATUS_NOT_IMPLEMENTED;
    try {
        const auto message = abi::v1::format_feature_failure(feature, context);
        const bool delivered = sink_.write == nullptr ? write_stderr(message) : sink_.write(sink_.context, message);
        if (!delivered) {
            status_ = EAOT_V1_STATUS_DIAGNOSTIC_FAILURE;
        }
    } catch (...) {
        status_ = EAOT_V1_STATUS_DIAGNOSTIC_FAILURE;
    }
    return status_;
}
} // namespace erlang_aot::runtime

#pragma once
#include <erlang_aot/abi/feature_diagnostic.hpp>
#include <erlang_aot/abi/status.h>

namespace erlang_aot::runtime {
struct DiagnosticSink {
    // Borrow callback state for the report; the caller must retain it through invocation.
    void *context = nullptr;
    // Consume a fully formatted message without newline; null selects stderr. False means delivery failed.
    bool (*write)(void *, std::string_view) = nullptr;
};

// One owner-side failure per operation; propagation reads status without reporting a second time.
// This host-side object is not a C ABI parameter and must not be shared across concurrent operations.
class FeatureFailure final {
  public:
    // Reserve a report without emitting anything; default delivery goes to stderr.
    explicit FeatureFailure(DiagnosticSink sink = {}) noexcept;
    FeatureFailure(const FeatureFailure &) = delete;
    FeatureFailure &operator=(const FeatureFailure &) = delete;
    FeatureFailure(FeatureFailure &&) = delete;
    FeatureFailure &operator=(FeatureFailure &&) = delete;
    ~FeatureFailure() = default;
    // Latch the first failure, translate sink/formatting exceptions and return an explicit C ABI status.
    eaot_v1_status report(abi::v1::FeatureId feature, const abi::v1::FeatureContext &context = {}) noexcept;
    // Inspect/propagate the existing outcome; OK means no failure has been reported by this owner.
    eaot_v1_status status() const noexcept;

  private:
    // Retain only a borrowed sink; no runtime-wide diagnostic state or LLVM dependency is required.
    DiagnosticSink sink_;
    // Prevent retries, reformatting and duplicate output while failure propagates through callers.
    eaot_v1_status status_ = EAOT_V1_STATUS_OK;
};
} // namespace erlang_aot::runtime

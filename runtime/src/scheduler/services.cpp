#include "state.hpp"

namespace erlang_aot::runtime {
SchedulerResult<void> SchedulerService::run(DiagnosticSink sink) noexcept {
    if (impl_->stopping) {
        return std::unexpected(SchedulerError::stopped);
    }
    return deferred_service<SchedulerError>(abi::v1::FeatureId::scheduling, "SchedulerService::run", sink);
}

SchedulerResult<StepResult> SchedulerService::execute(ProcessIdentity process, DiagnosticSink sink) noexcept {
    if (impl_->stopping) {
        return std::unexpected(SchedulerError::stopped);
    }
    const auto index = impl_->find(process);
    if (!index) {
        return std::unexpected(index.error());
    }
    const auto &lifecycle = impl_->entries[*index].lifecycle;
    if (lifecycle.state != ProcessState::runnable || lifecycle.suspended) {
        return std::unexpected(SchedulerError::invalid_transition);
    }
    return deferred_service<SchedulerError>(abi::v1::FeatureId::process_execution, "SchedulerService::execute", sink);
}
} // namespace erlang_aot::runtime

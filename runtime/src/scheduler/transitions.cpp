#include "state.hpp"

namespace erlang_aot::runtime {
namespace {
// Validate a reported return before changing the registry; malformed inputs leave the dispatch in flight.
SchedulerResult<ProcessState> returned_state(StepResult result) noexcept {
    switch (result.disposition) {
    case StepDisposition::yielded:
        return ProcessState::runnable;
    case StepDisposition::waiting:
        return ProcessState::waiting;
    case StepDisposition::exited:
        if (static_cast<unsigned>(result.reason) > static_cast<unsigned>(ExitReason::runtime_shutdown)) {
            return std::unexpected(SchedulerError::invalid_argument);
        }
        return ProcessState::exited;
    }
    return std::unexpected(SchedulerError::invalid_argument);
}
} // namespace

SchedulerResult<void> SchedulerService::begin_dispatch(ProcessIdentity process) noexcept {
    if (impl_->stopping) {
        return std::unexpected(SchedulerError::stopped);
    }
    const auto index = impl_->find(process);
    if (!index) {
        return std::unexpected(index.error());
    }
    auto &lifecycle = impl_->entries[*index].lifecycle;
    if (lifecycle.state != ProcessState::runnable || lifecycle.suspended) {
        return std::unexpected(SchedulerError::invalid_transition);
    }
    lifecycle.state = ProcessState::running;
    return {};
}

SchedulerResult<void> SchedulerService::finish_dispatch(ProcessIdentity process, StepResult result) noexcept {
    const auto index = impl_->find(process);
    if (!index) {
        return std::unexpected(index.error());
    }
    auto &lifecycle = impl_->entries[*index].lifecycle;
    if (lifecycle.state != ProcessState::running) {
        return std::unexpected(SchedulerError::invalid_transition);
    }
    const auto state = returned_state(result);
    if (!state) {
        return std::unexpected(state.error());
    }
    lifecycle.state = *state;
    if (*state == ProcessState::exited) {
        lifecycle.exit_reason = result.reason;
    }
    return {};
}

SchedulerResult<void> SchedulerService::set_suspended(ProcessIdentity process, bool suspended) noexcept {
    if (impl_->stopping) {
        return std::unexpected(SchedulerError::stopped);
    }
    const auto index = impl_->find(process);
    if (!index) {
        return std::unexpected(index.error());
    }
    auto &lifecycle = impl_->entries[*index].lifecycle;
    if (lifecycle.state == ProcessState::running || lifecycle.state == ProcessState::exited) {
        return std::unexpected(SchedulerError::invalid_transition);
    }
    lifecycle.suspended = suspended;
    return {};
}
} // namespace erlang_aot::runtime

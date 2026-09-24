#include "state.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
SchedulerService::SchedulerService(std::uint64_t runtime_identity) : impl_(std::make_unique<Impl>(runtime_identity)) {}

SchedulerService::~SchedulerService() = default;

SchedulerResult<std::size_t> SchedulerService::Impl::find(ProcessIdentity process) const noexcept {
    if (process.runtime_ != runtime_identity) {
        return std::unexpected(SchedulerError::wrong_owner);
    }
    const auto found = std::ranges::find(entries, process, &Entry::identity);
    if (found == entries.end()) {
        return std::unexpected(SchedulerError::unknown_process);
    }
    return static_cast<std::size_t>(found - entries.begin());
}

SchedulerResult<void> SchedulerService::register_process(ProcessContext &context) noexcept {
    if (impl_->stopping) {
        return std::unexpected(SchedulerError::stopped);
    }
    if (context.identity().runtime_ != impl_->runtime_identity) {
        return std::unexpected(SchedulerError::wrong_owner);
    }
    if (context.scheduler_registered_once_) {
        return std::unexpected(SchedulerError::already_registered);
    }
    try {
        impl_->entries.push_back({context.identity(), {}});
        context.scheduler_registered_once_ = true;
        return {};
    } catch (const std::bad_alloc &) {
        return std::unexpected(SchedulerError::resource_limit);
    } catch (const std::length_error &) {
        return std::unexpected(SchedulerError::resource_limit);
    }
}

SchedulerResult<void> SchedulerService::remove_process(ProcessIdentity process) noexcept {
    const auto index = impl_->find(process);
    if (!index) {
        return std::unexpected(index.error());
    }
    if (impl_->entries[*index].lifecycle.state == ProcessState::running) {
        return std::unexpected(SchedulerError::invalid_transition);
    }
    impl_->entries.erase(impl_->entries.begin() + static_cast<std::ptrdiff_t>(*index));
    return {};
}

SchedulerResult<ProcessLifecycle> SchedulerService::inspect(ProcessIdentity process) const noexcept {
    const auto index = impl_->find(process);
    if (!index) {
        return std::unexpected(index.error());
    }
    return impl_->entries[*index].lifecycle;
}

std::size_t SchedulerService::process_count() const noexcept { return impl_->entries.size(); }

void SchedulerService::request_shutdown() noexcept { impl_->stopping = true; }

bool SchedulerService::stopping() const noexcept { return impl_->stopping; }

void SchedulerService::clear() noexcept {
    request_shutdown();
    impl_->entries.clear();
}
} // namespace erlang_aot::runtime

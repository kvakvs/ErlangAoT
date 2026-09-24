#include "../memory/heap_policy.hpp"
#include "../runtime_state.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
using abi::v1::Status;

std::expected<ProcessContext *, Status> Runtime::create_context(HeapOptions options) noexcept {
    if (!impl_) {
        return std::unexpected(Status::stopped);
    }
    if (!detail::valid_heap_options(options)) {
        return std::unexpected(Status::invalid_argument);
    }
    if (impl_->contexts.size() >= impl_->options.max_contexts ||
        impl_->next_context == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(Status::resource_limit);
    }
    try {
        auto context = std::unique_ptr<ProcessContext>(
            new ProcessContext(*this, ProcessIdentity({impl_->identity}, impl_->next_context), options));
        auto *borrowed = context.get();
        impl_->contexts.push_back(std::move(context));
        ++impl_->next_context;
        return borrowed;
    } catch (const std::bad_alloc &) {
        return std::unexpected(Status::out_of_memory);
    } catch (const std::length_error &) {
        return std::unexpected(Status::resource_limit);
    } catch (...) {
        return std::unexpected(Status::internal_error);
    }
}

Status Runtime::destroy_context(ProcessContext *context) noexcept {
    if (!impl_) {
        return Status::stopped;
    }
    if (context == nullptr) {
        return Status::invalid_argument;
    }
    const auto found =
        std::ranges::find_if(impl_->contexts, [context](const auto &owner) { return owner.get() == context; });
    if (found == impl_->contexts.end()) {
        return Status::wrong_owner;
    }
    const auto removed = impl_->scheduler.remove_process(context->identity());
    if (!removed && removed.error() != SchedulerError::unknown_process) {
        return Status::busy;
    }
    impl_->contexts.erase(found);
    return Status::ok;
}
} // namespace erlang_aot::runtime

#include "../memory/heap_policy.hpp"
#include "../runtime_state.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
using abi::v1::Status;

std::expected<ProcessContext *, Status> Runtime::create_context() noexcept {
    if (!impl_) {
        return std::unexpected(Status::stopped);
    }
    return create_context(impl_->options.process_heap, impl_->options.process_stack);
}

std::expected<ProcessContext *, Status> Runtime::create_context(HeapOptions options,
                                                                StackOptions stack_options) noexcept {
    if (!impl_) {
        return std::unexpected(Status::stopped);
    }
    if (!detail::valid_heap_options(options)) {
        return std::unexpected(Status::invalid_argument);
    }
    // Process count is unlimited; only the never-recycled pid number sequence can run out.
    const auto number = impl_->process_numbers.issue();
    if (!number) {
        return std::unexpected(number.error() == TermError::out_of_memory ? Status::out_of_memory
                                                                          : Status::resource_limit);
    }
    try {
        auto context = std::unique_ptr<ProcessContext>(
            new ProcessContext(*this, ProcessIdentity({impl_->identity}, *number), options, stack_options,
                               impl_->memory, impl_->process_numbers));
        auto *borrowed = context.get();
        impl_->contexts.push_back(std::move(context));
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

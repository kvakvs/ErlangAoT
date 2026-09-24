#include "../runtime_state.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace erlang_aot::runtime {
namespace {
// Validate byte budgets before creating owners; no backing memory is allocated by this milestone.
bool valid_heap_options(HeapOptions options) noexcept {
    return options.chunk_bytes != 0 && options.chunk_bytes <= options.limit_bytes &&
           options.chunk_bytes % sizeof(Word) == 0 && options.limit_bytes % sizeof(Word) == 0;
}
} // namespace

std::expected<ProcessContext *, eaot_v1_status> Runtime::create_context(HeapOptions options) noexcept {
    if (!impl_) {
        return std::unexpected(EAOT_V1_STATUS_STOPPED);
    }
    if (!valid_heap_options(options)) {
        return std::unexpected(EAOT_V1_STATUS_INVALID_ARGUMENT);
    }
    if (impl_->contexts.size() >= impl_->options.max_contexts ||
        impl_->next_context == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(EAOT_V1_STATUS_RESOURCE_LIMIT);
    }
    try {
        auto context = std::unique_ptr<ProcessContext>(
            new ProcessContext(*this, ProcessIdentity({impl_->identity}, impl_->next_context), options));
        auto *borrowed = context.get();
        impl_->contexts.push_back(std::move(context));
        ++impl_->next_context;
        return borrowed;
    } catch (const std::bad_alloc &) {
        return std::unexpected(EAOT_V1_STATUS_OUT_OF_MEMORY);
    } catch (const std::length_error &) {
        return std::unexpected(EAOT_V1_STATUS_RESOURCE_LIMIT);
    } catch (...) {
        return std::unexpected(EAOT_V1_STATUS_INTERNAL_ERROR);
    }
}

eaot_v1_status Runtime::destroy_context(ProcessContext *context) noexcept {
    if (!impl_) {
        return EAOT_V1_STATUS_STOPPED;
    }
    if (context == nullptr) {
        return EAOT_V1_STATUS_INVALID_ARGUMENT;
    }
    const auto found =
        std::ranges::find_if(impl_->contexts, [context](const auto &owner) { return owner.get() == context; });
    if (found == impl_->contexts.end()) {
        return EAOT_V1_STATUS_WRONG_OWNER;
    }
    impl_->contexts.erase(found);
    return EAOT_V1_STATUS_OK;
}
} // namespace erlang_aot::runtime

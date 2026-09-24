#include "runtime_state.hpp"
#include <atomic>
#include <limits>
#include <new>
#include <utility>

namespace erlang_aot::runtime {
namespace {
// Reserve non-recycled runtime identities, including concurrently created independent host instances.
std::expected<std::uint64_t, eaot_v1_status> reserve_identity() noexcept {
    static std::atomic<std::uint64_t> next{1};
    auto candidate = next.load(std::memory_order_relaxed);
    do {
        if (candidate == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(EAOT_V1_STATUS_RESOURCE_LIMIT);
        }
    } while (!next.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed));
    return candidate;
}
} // namespace

Runtime::Impl::Impl(RuntimeOptions options, std::uint64_t identity) : options(options), identity(identity) {}

Runtime::Runtime(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Runtime::~Runtime() = default;

std::expected<std::unique_ptr<Runtime>, eaot_v1_status> Runtime::start(RuntimeOptions options) noexcept {
    if (options.max_contexts == 0) {
        return std::unexpected(EAOT_V1_STATUS_INVALID_ARGUMENT);
    }
    const auto identity = reserve_identity();
    if (!identity) {
        return std::unexpected(identity.error());
    }
    try {
        auto state = std::make_unique<Impl>(options, *identity);
        return std::unique_ptr<Runtime>(new Runtime(std::move(state)));
    } catch (const std::bad_alloc &) {
        return std::unexpected(EAOT_V1_STATUS_OUT_OF_MEMORY);
    } catch (...) {
        return std::unexpected(EAOT_V1_STATUS_INTERNAL_ERROR);
    }
}

eaot_v1_status Runtime::shutdown() noexcept {
    if (context_count() != 0) {
        return EAOT_V1_STATUS_BUSY;
    }
    impl_.reset();
    return EAOT_V1_STATUS_OK;
}

std::size_t Runtime::context_count() const noexcept { return impl_ ? impl_->contexts.size() : 0; }
} // namespace erlang_aot::runtime

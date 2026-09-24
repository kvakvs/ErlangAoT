#include "runtime_state.hpp"
#include <atomic>
#include <limits>
#include <new>
#include <utility>

namespace erlang_aot::runtime {
using abi::v1::Status;

namespace {
// Reserve non-recycled runtime identities, including concurrently created independent host instances.
std::expected<std::uint64_t, Status> reserve_identity() noexcept {
    static std::atomic<std::uint64_t> next{1};
    auto candidate = next.load(std::memory_order_relaxed);
    do {
        if (candidate == std::numeric_limits<std::uint64_t>::max()) {
            return std::unexpected(Status::resource_limit);
        }
    } while (!next.compare_exchange_weak(candidate, candidate + 1, std::memory_order_relaxed));
    return candidate;
}
} // namespace

Runtime::Impl::Impl(RuntimeOptions options, std::uint64_t identity) : options(options), identity(identity) {}

Runtime::Runtime(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Runtime::~Runtime() = default;

std::expected<std::unique_ptr<Runtime>, Status> Runtime::start(RuntimeOptions options) noexcept {
    if (options.abi_version != abi::v1::version || options.term_bits != sizeof(abi::v1::TermWord) * 8) {
        return std::unexpected(Status::abi_mismatch);
    }
    if (options.max_contexts == 0) {
        return std::unexpected(Status::invalid_argument);
    }
    const auto identity = reserve_identity();
    if (!identity) {
        return std::unexpected(identity.error());
    }
    try {
        auto state = std::make_unique<Impl>(options, *identity);
        return std::unique_ptr<Runtime>(new Runtime(std::move(state)));
    } catch (const std::bad_alloc &) {
        return std::unexpected(Status::out_of_memory);
    } catch (...) {
        return std::unexpected(Status::internal_error);
    }
}

Status Runtime::shutdown() noexcept {
    if (context_count() != 0) {
        return Status::busy;
    }
    impl_.reset();
    return Status::ok;
}

std::size_t Runtime::context_count() const noexcept { return impl_ ? impl_->contexts.size() : 0; }
} // namespace erlang_aot::runtime

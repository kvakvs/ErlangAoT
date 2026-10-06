#include "memory/heap_policy.hpp"
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

// Explicit count construction keeps Debug STL proxy allocation failures catchable during startup.
Runtime::Impl::Impl(RuntimeOptions options, std::uint64_t identity)
    : atom_storage(options.max_atoms), scheduler(identity), options(options), identity(identity),
      memory(std::make_shared<detail::RuntimeMemory>(options.memory_limit_bytes / sizeof(Word))), contexts(0) {}

Runtime::Impl::~Impl() { scheduler.clear(); }

Runtime::Runtime(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

Runtime::~Runtime() = default;

std::expected<std::unique_ptr<Runtime>, Status> Runtime::start(RuntimeOptions options) noexcept {
    if (options.abi_version != abi::v1::version || options.term_bits != sizeof(abi::v1::TermWord) * 8) {
        return std::unexpected(Status::abi_mismatch);
    }
    if (options.max_atoms == 0 || options.max_atoms > AtomStorage::hard_limit ||
        !detail::valid_heap_options(options.process_heap)) {
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

CodeServer *Runtime::code_server() noexcept { return impl_ ? &impl_->code_server : nullptr; }

AtomStorage *Runtime::atom_storage() noexcept { return impl_ ? &impl_->atom_storage : nullptr; }

SchedulerService *Runtime::scheduler() noexcept { return impl_ ? &impl_->scheduler : nullptr; }

OutputSink Runtime::standard_output() const noexcept { return impl_ ? impl_->options.standard_output : OutputSink{}; }

std::size_t Runtime::context_count() const noexcept { return impl_ ? impl_->contexts.size() : 0; }

std::size_t Runtime::memory_bytes() const noexcept { return impl_ ? impl_->memory->used() * sizeof(Word) : 0; }
} // namespace erlang_aot::runtime

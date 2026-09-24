#pragma once
#include "process_context.hpp"
#include <erlang_aot/abi/status.hpp>
#include <expected>

namespace erlang_aot::runtime {
struct RuntimeOptions {
    // Bound live context owners independently of future heap and atom budgets.
    std::size_t max_contexts = 1024;
    // Reject a caller built for a different project ABI before publishing runtime state.
    std::uint32_t abi_version = abi::v1::version;
    // Keep the native runtime term width explicit for project compatibility checks.
    std::uint32_t term_bits = sizeof(abi::v1::TermWord) * 8;
};

// Own stable process contexts and reserved runtime-wide services; calls require host-side serialization.
class Runtime final {
  public:
    // Construct privately and publish only a fully initialized owner; contain allocation failures.
    static std::expected<std::unique_ptr<Runtime>, abi::v1::Status> start(RuntimeOptions options = {}) noexcept;
    // Invalidate/destroy any remaining contexts before runtime-wide services as a host RAII fallback.
    ~Runtime();
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;
    Runtime(Runtime &&) = delete;
    Runtime &operator=(Runtime &&) = delete;
    // Stop only once all contexts are gone; BUSY leaves admission and existing contexts unchanged.
    abi::v1::Status shutdown() noexcept;
    // Create a stable runtime-owned context with lazy storage and a unique, never-recycled identity.
    std::expected<ProcessContext *, abi::v1::Status> create_context(HeapOptions options = {}) noexcept;
    // Remove only a context owned by this runtime; foreign pointers are compared without dereferencing.
    abi::v1::Status destroy_context(ProcessContext *context) noexcept;
    // Observe active context ownership without claiming scheduler/process execution support.
    std::size_t context_count() const noexcept;

  private:
    // Retain contexts and service reservations independently of the public C++/generated ABI layout.
    class Impl;
    std::unique_ptr<Impl> impl_;
    // Adopt fully constructed state so startup failures unwind before publication.
    explicit Runtime(std::unique_ptr<Impl> impl) noexcept;
};
} // namespace erlang_aot::runtime

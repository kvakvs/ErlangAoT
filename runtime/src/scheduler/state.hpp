#pragma once
#include <erlang_aot/runtime/scheduler.hpp>
#include <vector>

namespace erlang_aot::runtime {
class SchedulerService::Impl final {
  public:
    // Fix registry ownership before accepting the first process identity.
    explicit Impl(std::uint64_t identity) : runtime_identity(identity) {}

    // Resolve only this runtime's identities; returned indices are borrowed within a serialized call.
    SchedulerResult<std::size_t> find(ProcessIdentity process) const noexcept;

    struct Entry final {
        // Retain an immutable key, never a borrowed context or heap pointer.
        ProcessIdentity identity;
        // Keep execution state and suspension together through registry growth.
        ProcessLifecycle lifecycle;
    };

    // Reject identities issued by other runtime instances before registry lookup.
    std::uint64_t runtime_identity;
    // Close new work while still allowing explicit removal and in-flight return bookkeeping.
    bool stopping = false;
    // Own bounded-by-context-count records; allocation failure never publishes a partial entry.
    std::vector<Entry> entries;
};
} // namespace erlang_aot::runtime

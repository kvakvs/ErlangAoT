#pragma once
#include <clause/runtime/scheduler.hpp>
#include <vector>

namespace clause::runtime {
class SchedulerService::Impl final {
  public:
    // Fix ownership; empty-range construction lets Debug STL proxy allocation failures propagate.
    explicit Impl(std::uint64_t identity) : runtime_identity(identity), entries(std::initializer_list<Entry>{}) {}

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
} // namespace clause::runtime

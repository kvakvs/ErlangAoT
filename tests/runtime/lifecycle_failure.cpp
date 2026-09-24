#include <cstdlib>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

using erlang_aot::abi::v1::Status;
using erlang_aot::runtime::Runtime;

namespace {
// Only this isolated test executable replaces allocation; production has no failpoint hooks.
std::size_t remaining = std::numeric_limits<std::size_t>::max();
// Count live allocations to prove cleanup at every partially completed construction boundary.
std::size_t live_allocations = 0;
} // namespace

// Fail a chosen allocation ordinal before delegating to the host allocator.
void *operator new(std::size_t size) {
    if (remaining == 0) {
        throw std::bad_alloc();
    }
    if (remaining != std::numeric_limits<std::size_t>::max()) {
        --remaining;
    }
    void *memory = std::malloc(size == 0 ? 1 : size);
    if (memory == nullptr) {
        throw std::bad_alloc();
    }
    ++live_allocations;
    return memory;
}

// Balance tracked single-object allocations, including failed shared-token/control-block construction.
void operator delete(void *memory) noexcept {
    if (memory != nullptr) {
        --live_allocations;
        std::free(memory);
    }
}

// Compilers selecting sized deallocation must balance the same tracked allocation count.
void operator delete(void *memory, std::size_t) noexcept { ::operator delete(memory); }

// Keep failure diagnostics outside the allocation-injection window.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Every startup allocation must roll back before an owner can escape to the host.
void check_startup() {
    bool succeeded = false;
    for (std::size_t ordinal = 0; ordinal < 32 && !succeeded; ++ordinal) {
        const auto baseline = live_allocations;
        remaining = ordinal;
        auto runtime = Runtime::start();
        remaining = std::numeric_limits<std::size_t>::max();
        succeeded = runtime.has_value();
        if (succeeded) {
            require((*runtime)->shutdown() == Status::ok, "startup cleanup failed");
            runtime->reset();
        } else {
            require(runtime.error() == Status::out_of_memory, "wrong startup failure");
        }
        require(live_allocations == baseline, "partial runtime initialization leaked");
    }
    require(succeeded, "startup failpoint sweep never reached success");
}

// Fail each context/mailbox/token/registry allocation; existing contexts must survive every rollback.
void check_context_creation() {
    bool succeeded = false;
    for (std::size_t ordinal = 0; ordinal < 32 && !succeeded; ++ordinal) {
        const auto baseline = live_allocations;
        auto runtime = Runtime::start();
        require(runtime.has_value(), "fixture startup failed");
        auto existing = (*runtime)->create_context();
        require(existing.has_value(), "fixture context failed");
        const auto retained = live_allocations;
        remaining = ordinal;
        auto created = (*runtime)->create_context();
        remaining = std::numeric_limits<std::size_t>::max();
        succeeded = created.has_value();
        if (succeeded) {
            require((*runtime)->destroy_context(*created) == Status::ok, "new context cleanup failed");
        } else {
            require(created.error() == Status::out_of_memory, "wrong context failure");
            require(live_allocations == retained, "partial context initialization leaked");
        }
        require((*runtime)->destroy_context(*existing) == Status::ok, "failed creation damaged existing context");
        require((*runtime)->shutdown() == Status::ok, "runtime cleanup failed");
        runtime->reset();
        require(live_allocations == baseline, "failed construction retained memory after shutdown");
    }
    require(succeeded, "context failpoint sweep never reached success");
}

// An isolated allocator override verifies real failure cleanup without adding production test switches.
int main() {
    try {
        check_startup();
        check_context_creation();
    } catch (const std::exception &error) {
        remaining = std::numeric_limits<std::size_t>::max();
        std::cerr << error.what() << '\n';
        return 1;
    }
}

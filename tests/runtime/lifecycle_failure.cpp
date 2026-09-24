#include <cstdlib>
#include <erlang_aot/abi/runtime.h>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

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

// Every startup allocation must roll back before a handle can escape to the host.
void check_startup() {
    bool succeeded = false;
    for (std::size_t ordinal = 0; ordinal < 32 && !succeeded; ++ordinal) {
        eaot_v1_runtime *runtime = nullptr;
        const auto baseline = live_allocations;
        remaining = ordinal;
        const auto status = eaot_v1_runtime_start(nullptr, &runtime);
        remaining = std::numeric_limits<std::size_t>::max();
        succeeded = status == EAOT_V1_STATUS_OK;
        if (!succeeded) {
            require(status == EAOT_V1_STATUS_OUT_OF_MEMORY && runtime == nullptr, "startup failure published a handle");
        }
        require(eaot_v1_runtime_shutdown(&runtime) == EAOT_V1_STATUS_OK, "startup cleanup failed");
        require(live_allocations == baseline, "partial runtime initialization leaked");
    }
    require(succeeded, "startup failpoint sweep never reached success");
}

// Fail each context/mailbox/token/registry allocation; existing contexts must survive every rollback.
void check_context_creation() {
    bool succeeded = false;
    for (std::size_t ordinal = 0; ordinal < 32 && !succeeded; ++ordinal) {
        const auto baseline = live_allocations;
        eaot_v1_runtime *runtime = nullptr;
        eaot_v1_context *existing = nullptr;
        eaot_v1_context *created = nullptr;
        require(eaot_v1_runtime_start(nullptr, &runtime) == EAOT_V1_STATUS_OK, "fixture startup failed");
        require(eaot_v1_context_create(runtime, nullptr, &existing) == EAOT_V1_STATUS_OK, "fixture context failed");
        const auto retained = live_allocations;
        remaining = ordinal;
        const auto status = eaot_v1_context_create(runtime, nullptr, &created);
        remaining = std::numeric_limits<std::size_t>::max();
        succeeded = status == EAOT_V1_STATUS_OK;
        if (!succeeded) {
            require(status == EAOT_V1_STATUS_OUT_OF_MEMORY && created == nullptr, "context failure published a handle");
            require(live_allocations == retained, "partial context initialization leaked");
        }
        require(eaot_v1_context_destroy(runtime, &created) == EAOT_V1_STATUS_OK, "new context cleanup failed");
        require(eaot_v1_context_destroy(runtime, &existing) == EAOT_V1_STATUS_OK,
                "failed creation damaged existing context");
        require(eaot_v1_runtime_shutdown(&runtime) == EAOT_V1_STATUS_OK, "runtime cleanup failed");
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

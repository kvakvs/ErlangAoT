#include <cstdlib>
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <erlang_aot/runtime/scheduler.hpp>
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

// Memory service rejection and immediate copying must work even when host allocation is exhausted.
void check_heap_without_allocation(erlang_aot::runtime::ProcessHeap &heap) {
    using namespace erlang_aot::runtime;
    const auto baseline = live_allocations;
    remaining = 0;
    const auto allocation = heap.allocate(1);
    const auto collection = heap.collect();
    const auto copied = Term::from_word(*encode_integer(7))->copy_to(heap);
    const auto invalid = heap.add(Term{});
    remaining = std::numeric_limits<std::size_t>::max();
    require(allocation == std::unexpected(HeapError::not_implemented), "allocation stub changed under OOM");
    require(collection == std::unexpected(HeapError::not_implemented), "collection stub changed under OOM");
    require(copied && copied->integer_value() == 7, "immediate copy allocated bookkeeping");
    require(invalid == std::unexpected(TermError::invalid_encoding), "invalid copy changed under OOM");
    require(live_allocations == baseline, "memory boundary retained allocations");
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
        check_heap_without_allocation((*existing)->heap());
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

// Fail registry key/type-vector/node allocations without disturbing existing signatures.
void check_registry_creation() {
    using namespace erlang_aot::runtime;
    bool succeeded = false;
    for (std::size_t ordinal = 0; ordinal < 32 && !succeeded; ++ordinal) {
        const auto baseline = live_allocations;
        {
            ModuleRegistry registry;
            const Callable target = [](ProcessContext &, std::span<const Term>) { return CallResult<Term>(Term{}); };
            require(registry.add("existing", 0, Callable{target}).has_value(), "registry fixture failed");
            const auto retained = live_allocations;
            remaining = ordinal;
            auto added = registry.add("allocation_failure_signature", 2, Callable{target});
            remaining = std::numeric_limits<std::size_t>::max();
            succeeded = added.has_value();
            if (!succeeded) {
                require(added.error() == RegistryError::resource_limit, "wrong registry failure");
                require(live_allocations == retained, "partial signature leaked");
                require(!registry.find("allocation_failure_signature", 2), "failed signature published");
            }
            require(registry.find("existing", 0).has_value(), "failed add damaged existing signature");
        }
        require(live_allocations == baseline, "registry cleanup leaked");
    }
    require(succeeded, "registry allocation sweep never succeeded");
}

// Sweep publication allocations, proving neither partial modules nor leaked captures survive failure.
void check_module_publication() {
    using namespace erlang_aot::runtime;
    bool succeeded = false;
    for (std::size_t ordinal = 0; ordinal < 32 && !succeeded; ++ordinal) {
        const auto baseline = live_allocations;
        {
            CodeServer server;
            require(server.load({"existing", CodeImage::linked(), std::make_unique<ModuleRegistry>()}).has_value(),
                    "module fixture failed");
            const auto retained = live_allocations;
            ModuleDefinition definition{"allocation_failure_module", CodeImage::linked(),
                                        std::make_unique<ModuleRegistry>()};
            remaining = ordinal;
            auto loaded = server.load(std::move(definition));
            remaining = std::numeric_limits<std::size_t>::max();
            succeeded = loaded.has_value();
            if (!succeeded) {
                require(loaded.error() == CodeError::resource_limit, "wrong publication failure");
                require(live_allocations == retained, "failed module retained draft storage");
                require(!server.find_module("allocation_failure_module"), "failed module published");
            }
            require(server.find_module("existing").has_value(), "failed publication damaged existing module");
        }
        require(live_allocations == baseline, "module cleanup leaked");
    }
    require(succeeded, "module allocation sweep never succeeded");
}

// Failed registry growth must leave both the entry and the once-only context marker unpublished.
void check_scheduler_registration() {
    using namespace erlang_aot::runtime;
    const auto baseline = live_allocations;
    auto runtime = Runtime::start();
    require(runtime.has_value(), "scheduler fixture startup failed");
    auto context = (*runtime)->create_context();
    require(context.has_value(), "scheduler fixture context failed");
    auto &scheduler = *(*runtime)->scheduler();
    const auto retained = live_allocations;
    remaining = 0;
    const auto failed = scheduler.register_process(**context);
    remaining = std::numeric_limits<std::size_t>::max();
    require(failed == std::unexpected(SchedulerError::resource_limit), "wrong scheduler allocation failure");
    require(live_allocations == retained && scheduler.process_count() == 0, "partial registration retained resources");
    require(scheduler.register_process(**context).has_value(), "failed registration consumed once-only identity");
    require((*runtime)->destroy_context(*context) == Status::ok, "registered context cleanup failed");
    require(scheduler.process_count() == 0, "context cleanup retained registration");
    runtime->reset();
    require(live_allocations == baseline, "scheduler cleanup leaked");
}

// An isolated allocator override verifies real failure cleanup without adding production test switches.
int main() {
    try {
        check_startup();
        check_context_creation();
        check_registry_creation();
        check_module_publication();
        check_scheduler_registration();
    } catch (const std::exception &error) {
        remaining = std::numeric_limits<std::size_t>::max();
        std::cerr << error.what() << '\n';
        return 1;
    }
}

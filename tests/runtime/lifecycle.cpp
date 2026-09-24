#include <erlang_aot/abi/runtime.h>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot::runtime;

// Keep contract assertions active in release builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Lazy storage owners and immutable identities must remain distinct before any heap operations exist.
void check_storage(ProcessContext &first, ProcessContext &second, ProcessContext &foreign) {
    require(&first.heap() != &second.heap() && &first.mailbox() != &second.mailbox(), "storage owners alias");
    require(first.identity() != second.identity() && first.identity() != foreign.identity(), "identities collide");
    require(first.heap().used_words() == 0 && first.heap().capacity_words() == 0, "lazy heap allocated storage");
}

// Contexts own distinct lazy storage and stable identities, even across independent runtime instances.
void check_owners() {
    auto started = Runtime::start({2});
    auto other = Runtime::start();
    require(started && other, "runtime startup failed");
    auto &runtime = **started;
    const auto first = runtime.create_context();
    const auto second = runtime.create_context();
    const auto foreign = (*other)->create_context();
    require(first && second && foreign, "context creation failed");
    check_storage(**first, **second, **foreign);
    require(runtime.destroy_context(*foreign) == EAOT_V1_STATUS_WRONG_OWNER, "foreign context accepted");
    require(runtime.context_count() == 2 && (*other)->context_count() == 1, "foreign rejection changed ownership");
    require(runtime.create_context() == std::unexpected(EAOT_V1_STATUS_RESOURCE_LIMIT), "context limit ignored");
    const auto identity = (*first)->identity();
    auto lifetime = (*first)->lifetime().lock();
    require(lifetime && lifetime->alive(), "live token unavailable");
    require(runtime.destroy_context(*first) == EAOT_V1_STATUS_OK && !lifetime->alive(),
            "exit did not invalidate token");
    auto replacement = runtime.create_context();
    require(replacement && (*replacement)->identity() != identity, "process identity recycled after exit");
}

// Explicit shutdown refuses live contexts; RAII destruction still invalidates survivors before releasing storage.
void check_shutdown() {
    auto started = Runtime::start();
    require(started.has_value(), "runtime startup failed");
    auto &runtime = **started;
    auto context = runtime.create_context();
    require(context.has_value(), "context startup failed");
    const auto weak = (*context)->lifetime();
    auto token = weak.lock();
    require(runtime.shutdown() == EAOT_V1_STATUS_BUSY && token->alive(), "busy shutdown changed liveness");
    require(runtime.destroy_context(*context) == EAOT_V1_STATUS_OK, "context teardown failed");
    require(!token->alive(), "teardown left a live host binding");
    token.reset();
    require(weak.expired(), "context retained lifetime storage after exit");
    require(runtime.shutdown() == EAOT_V1_STATUS_OK && runtime.shutdown() == EAOT_V1_STATUS_OK,
            "shutdown not idempotent");
    require(runtime.create_context() == std::unexpected(EAOT_V1_STATUS_STOPPED), "stopped runtime admitted context");
    auto automatic = Runtime::start();
    require(automatic.has_value(), "second runtime startup failed");
    auto remaining = (*automatic)->create_context();
    require(remaining.has_value(), "remaining context startup failed");
    auto survivor = (*remaining)->lifetime().lock();
    automatic->reset();
    require(survivor && !survivor->alive(), "RAII shutdown left dangling live bindings");
}

// Invalid budgets fail before publishing a process and leave later valid operations usable.
void check_options() {
    require(Runtime::start({0}) == std::unexpected(EAOT_V1_STATUS_INVALID_ARGUMENT), "invalid runtime limit accepted");
    auto started = Runtime::start();
    require(started.has_value(), "runtime startup failed");
    for (const HeapOptions options :
         {HeapOptions{0, 1024}, HeapOptions{16, 8}, HeapOptions{1, 1024}, HeapOptions{8, 9}}) {
        require((*started)->create_context(options) == std::unexpected(EAOT_V1_STATUS_INVALID_ARGUMENT),
                "invalid heap budget accepted");
    }
    require((*started)->context_count() == 0, "failed initialization published a context");
    require((*started)->destroy_context(nullptr) == EAOT_V1_STATUS_INVALID_ARGUMENT, "null context accepted");
    require((*started)->create_context({sizeof(eaot_v1_term), sizeof(eaot_v1_term)}).has_value(),
            "valid minimal budget rejected");
}

// C callers receive explicit errors and retain non-null outputs/foreign handles on failure.
void check_c_errors() {
    eaot_v1_runtime *runtime = nullptr;
    eaot_v1_context *context = nullptr;
    require(eaot_v1_runtime_start(nullptr, nullptr) == EAOT_V1_STATUS_INVALID_ARGUMENT, "null output accepted");
    eaot_v1_runtime_options options{EAOT_ABI_VERSION + 1, sizeof(eaot_v1_term) * 8, 2};
    require(eaot_v1_runtime_start(&options, &runtime) == EAOT_V1_STATUS_ABI_MISMATCH && runtime == nullptr,
            "wrong ABI accepted");
    options.abi_version = EAOT_ABI_VERSION;
    options.term_bits = sizeof(eaot_v1_term) == 8 ? 32 : 64;
    require(eaot_v1_runtime_start(&options, &runtime) == EAOT_V1_STATUS_ABI_MISMATCH, "wrong word width accepted");
    require(eaot_v1_context_create(nullptr, nullptr, &context) == EAOT_V1_STATUS_INVALID_ARGUMENT,
            "null runtime accepted");
    require(eaot_v1_runtime_start(nullptr, &runtime) == EAOT_V1_STATUS_OK, "C startup failed");
    require(eaot_v1_runtime_start(nullptr, &runtime) == EAOT_V1_STATUS_INVALID_ARGUMENT, "live output overwritten");
    require(eaot_v1_context_create(runtime, nullptr, &context) == EAOT_V1_STATUS_OK, "C context creation failed");
    require(eaot_v1_context_create(runtime, nullptr, &context) == EAOT_V1_STATUS_INVALID_ARGUMENT,
            "live context overwritten");
    require(eaot_v1_runtime_shutdown(&runtime) == EAOT_V1_STATUS_BUSY && runtime != nullptr,
            "C busy shutdown freed runtime");
    require(eaot_v1_context_destroy(runtime, &context) == EAOT_V1_STATUS_OK && context == nullptr,
            "C context destruction failed");
    require(eaot_v1_runtime_shutdown(&runtime) == EAOT_V1_STATUS_OK && runtime == nullptr, "C shutdown failed");
}

// Repeated independent C lifecycles include foreign-owner rejection and null-handle idempotence.
void check_c_lifecycles() {
    for (unsigned iteration = 0; iteration < 32; ++iteration) {
        eaot_v1_runtime *first = nullptr;
        eaot_v1_runtime *second = nullptr;
        eaot_v1_context *context = nullptr;
        require(eaot_v1_runtime_start(nullptr, &first) == EAOT_V1_STATUS_OK, "first startup failed");
        require(eaot_v1_runtime_start(nullptr, &second) == EAOT_V1_STATUS_OK, "second startup failed");
        require(eaot_v1_context_create(first, nullptr, &context) == EAOT_V1_STATUS_OK, "context startup failed");
        require(eaot_v1_context_destroy(second, &context) == EAOT_V1_STATUS_WRONG_OWNER && context != nullptr,
                "foreign destroy corrupted handle");
        require(eaot_v1_context_destroy(first, &context) == EAOT_V1_STATUS_OK, "destroy failed");
        require(eaot_v1_context_destroy(first, &context) == EAOT_V1_STATUS_OK, "repeated destroy failed");
        require(eaot_v1_runtime_shutdown(&first) == EAOT_V1_STATUS_OK, "first shutdown failed");
        require(eaot_v1_runtime_shutdown(&second) == EAOT_V1_STATUS_OK, "second shutdown failed");
        require(eaot_v1_runtime_shutdown(&first) == EAOT_V1_STATUS_OK, "repeated shutdown failed");
    }
}

// Successful lifecycle and status-returning failures must remain silent until a host chooses to report them.
int main() {
    try {
        check_owners();
        check_shutdown();
        check_options();
        check_c_errors();
        check_c_lifecycles();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

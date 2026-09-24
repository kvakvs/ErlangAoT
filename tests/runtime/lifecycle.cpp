#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;
using erlang_aot::abi::v1::TermWord;

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
    require(runtime.destroy_context(*foreign) == Status::wrong_owner, "foreign context accepted");
    require(runtime.context_count() == 2 && (*other)->context_count() == 1, "foreign rejection changed ownership");
    require(runtime.create_context() == std::unexpected(Status::resource_limit), "context limit ignored");
    const auto identity = (*first)->identity();
    auto lifetime = (*first)->lifetime().lock();
    require(lifetime && lifetime->alive(), "live token unavailable");
    require(runtime.destroy_context(*first) == Status::ok && !lifetime->alive(), "exit did not invalidate token");
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
    require(runtime.shutdown() == Status::busy && token->alive(), "busy shutdown changed liveness");
    require(runtime.destroy_context(*context) == Status::ok, "context teardown failed");
    require(!token->alive(), "teardown left a live host binding");
    token.reset();
    require(weak.expired(), "context retained lifetime storage after exit");
    require(runtime.shutdown() == Status::ok && runtime.shutdown() == Status::ok, "shutdown not idempotent");
    require(runtime.create_context() == std::unexpected(Status::stopped), "stopped runtime admitted context");
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
    require(Runtime::start({0}) == std::unexpected(Status::invalid_argument), "invalid runtime limit accepted");
    auto started = Runtime::start();
    require(started.has_value(), "runtime startup failed");
    for (const HeapOptions options :
         {HeapOptions{0, 1024}, HeapOptions{16, 8}, HeapOptions{1, 1024}, HeapOptions{8, 9}}) {
        require((*started)->create_context(options) == std::unexpected(Status::invalid_argument),
                "invalid heap budget accepted");
    }
    require((*started)->context_count() == 0, "failed initialization published a context");
    require((*started)->destroy_context(nullptr) == Status::invalid_argument, "null context accepted");
    require((*started)->create_context({sizeof(TermWord), sizeof(TermWord)}).has_value(),
            "valid minimal budget rejected");
}

// Explicit version/width checks remain project contracts after removing the external C adapter.
void check_abi_options() {
    RuntimeOptions options;
    ++options.abi_version;
    require(Runtime::start(options) == std::unexpected(Status::abi_mismatch), "wrong ABI accepted");
    options.abi_version = erlang_aot::abi::v1::version;
    options.term_bits = sizeof(TermWord) == 8 ? 32 : 64;
    require(Runtime::start(options) == std::unexpected(Status::abi_mismatch), "wrong word width accepted");
}

// Repeated independent owners reject foreign contexts and shut down without retaining state.
void check_lifecycles() {
    for (unsigned iteration = 0; iteration < 32; ++iteration) {
        auto first = Runtime::start();
        auto second = Runtime::start();
        require(first && second, "independent startup failed");
        auto context = (*first)->create_context();
        require(context.has_value(), "context startup failed");
        require((*second)->destroy_context(*context) == Status::wrong_owner, "foreign destroy accepted");
        require((*first)->destroy_context(*context) == Status::ok, "destroy failed");
        require((*first)->shutdown() == Status::ok, "first shutdown failed");
        require((*second)->shutdown() == Status::ok, "second shutdown failed");
        require((*first)->shutdown() == Status::ok, "repeated shutdown failed");
    }
}

// Successful lifecycle and status-returning failures must remain silent until a host chooses to report them.
int main() {
    try {
        check_owners();
        check_shutdown();
        check_options();
        check_abi_options();
        check_lifecycles();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

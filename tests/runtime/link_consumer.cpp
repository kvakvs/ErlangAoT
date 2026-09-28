#include <array>
#include <cstdio>
#include <erlang_aot/abi/builtins.hpp>
#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <erlang_aot/runtime/scheduler.hpp>
#include <erlang_aot/runtime/terms.hpp>
#include <stdexcept>
#include <type_traits>

using erlang_aot::abi::v1::GeneratedFunction;
using erlang_aot::abi::v1::Status;
using erlang_aot::abi::v1::TermWord;
using erlang_aot::runtime::ProcessContext;
using erlang_aot::runtime::Runtime;
using namespace erlang_aot::runtime;

static_assert(std::is_same_v<erlang_aot::abi::v1::Context, ProcessContext>);
static_assert(std::is_same_v<std::underlying_type_t<Status>, std::uint8_t>);
static_assert(!std::is_convertible_v<Status, std::uint32_t>);

// A generated-entry-shaped project function borrows the real context through the forward-declared ABI type.
TermWord identity(ProcessContext *context, const TermWord *arguments) { return context == nullptr ? 0 : arguments[0]; }

// Fail at the public boundary while preserving useful subprocess diagnostics in Release too.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Rejected startup and context options must leave valid subsequent admission usable.
void invalid_options(Runtime &runtime) {
    require(Runtime::start({0}) == std::unexpected(Status::invalid_argument), "zero context limit accepted");
    RuntimeOptions options;
    ++options.abi_version;
    require(Runtime::start(options) == std::unexpected(Status::abi_mismatch), "ABI version mismatch accepted");
    options.abi_version = erlang_aot::abi::v1::version;
    options.term_bits = sizeof(TermWord) == 8 ? 32 : 64;
    require(Runtime::start(options) == std::unexpected(Status::abi_mismatch), "word width mismatch accepted");
    for (const HeapOptions heap : {HeapOptions{0, 1024}, HeapOptions{16, 8}, HeapOptions{1, 1024}, HeapOptions{8, 9}}) {
        require(runtime.create_context(heap) == std::unexpected(Status::invalid_argument), "invalid heap accepted");
    }
    require(runtime.context_count() == 0, "failed admission published context");
    require(runtime.destroy_context(nullptr) == Status::invalid_argument, "null context accepted");
}

// Rejected registry entries must not prevent subsequent valid publication and invocation.
void registry_failures(ModuleRegistry &registry, const Callable &copy) {
    require(registry.add("copy", 1, Callable{copy}) == std::unexpected(RegistryError::duplicate_key),
            "duplicate key accepted");
    require(registry.add("", 1, Callable{copy}) == std::unexpected(RegistryError::invalid_entry),
            "empty export accepted");
    require(registry.add("bad", 256, Callable{copy}) == std::unexpected(RegistryError::invalid_entry),
            "bad arity accepted");
    require(registry.add("bad", 1, {}) == std::unexpected(RegistryError::invalid_entry), "empty callable accepted");
    require(!registry.find("Copy", 1) && !registry.find("copy", 2), "lookup ignored exact key");
}

// Failed publication must preserve the already callable module and reject incomplete owners.
void publication_failures(CodeServer &server, ModuleRegistry &draft, const Callable &copy) {
    require(draft.add("later", 1, Callable{copy}) == std::unexpected(RegistryError::frozen),
            "published draft remained mutable");
    require(server.load({"consumer", CodeImage::linked(), std::make_unique<ModuleRegistry>()}) ==
                std::unexpected(CodeError::duplicate_module),
            "duplicate publication accepted");
    require(server.resolve({"consumer", "missing", 0}) == std::unexpected(CodeError::function_not_exported),
            "missing export resolved");
    require(!server.load({"invalid", {}, std::make_unique<ModuleRegistry>()}), "null image accepted");
    require(!server.load({"invalid", CodeImage::linked(), {}}), "null registry accepted");
}

// A published native function copies terms into its caller's context through the real runtime API.
ResolvedFunction install(Runtime &runtime) {
    auto registry = std::make_unique<ModuleRegistry>();
    const Callable copy = [](ProcessContext &context, std::span<const Term> arguments) {
        return CallResult<Term>(arguments.front().copy_to(context.heap()).value());
    };
    require(registry->add("copy", 1, Callable{copy}).has_value(), "registration failed");
    registry_failures(*registry, Callable{copy});
    require(registry
                ->add("copy", 0,
                      [](ProcessContext &, std::span<const Term>) {
                          return CallResult<Term>(Term::from_word(*encode_integer(42)).value());
                      })
                .has_value(),
            "distinct arity rejected");
    auto *draft = registry.get();
    require(runtime.code_server()->load({"consumer", CodeImage::linked(), std::move(registry)}).has_value(),
            "publication failed");
    publication_failures(*runtime.code_server(), *draft, Callable{copy});
    return runtime.code_server()->resolve({"consumer", "copy", 1}).value();
}

// Generated ABI, native registry, heap copying and dispatch must agree at every native integer boundary.
void roundtrips(ProcessContext &context, const ResolvedFunction &target) {
    require(context.code_server().resolve({"consumer", "copy", 0}).value().call(context, {})->integer_value() == 42,
            "zero-arity dispatch failed");
    using Encoding = erlang_aot::abi::v1::NativeIntegerEncoding;
    const std::array values{Encoding::minimum, Encoding::minimum + 1, std::int64_t{-42},
                            std::int64_t{-1},  std::int64_t{0},       std::int64_t{1},
                            std::int64_t{42},  Encoding::maximum - 1, Encoding::maximum};
    for (const auto value : values) {
        const auto word = encode_integer(value).value();
        require(word == Encoding::encode(value), "ABI/runtime encoding disagreement");
        GeneratedFunction *entry = identity;
        require(decode_integer(entry(&context, &word)) == value, "generated identity changed value");
        const std::array arguments{Term::from_word(word).value()};
        require(target.call(context, arguments)->integer_value() == value, "native copy changed value");
        TermWord output = 0;
        require(erlang_aot::abi::v1::dispatch_builtin(&context, "consumer", 8, "copy", 4, &word, 1, &output) ==
                    Status::ok,
                "dispatch bridge failed");
        require(output == word, "dispatch bridge changed encoding");
    }
    for (const Word word : {Word{0x2b}, Word{0x3b}}) {
        const std::array arguments{Term::from_word(word).value()};
        require(target.call(context, arguments)->word() == word, "empty immediate changed");
    }
    require(context.heap().used_words() == 0 && context.heap().capacity_words() == 0, "immediates allocated storage");
}

// Run actual calls between checked dispatch boundaries, then leave a waiter for explicit teardown.
void scheduled_calls(Runtime &runtime, ProcessContext &context, const ResolvedFunction &target) {
    auto &scheduler = *runtime.scheduler();
    const auto process = context.identity();
    const auto invalid = std::unexpected(SchedulerError::invalid_transition);
    require(scheduler.register_process(context).has_value(), "registration failed");
    require(runtime.shutdown() == Status::busy && !scheduler.stopping(), "busy shutdown closed admission");
    require(scheduler.set_suspended(process, true).has_value(), "suspend failed");
    require(scheduler.set_suspended(process, true).has_value(), "suspend not idempotent");
    require(scheduler.begin_dispatch(process) == invalid, "suspended dispatch accepted");
    require(scheduler.set_suspended(process, false).has_value(), "resume failed");
    require(scheduler.begin_dispatch(process).has_value(), "dispatch failed");
    require(runtime.destroy_context(&context) == Status::busy, "running context destroyed");
    roundtrips(context, target);
    require(scheduler.finish_dispatch(process, {}).has_value(), "yield failed");
    require(scheduler.inspect(process)->state == ProcessState::runnable, "yield did not restore runnable state");
    require(scheduler.begin_dispatch(process).has_value(), "redispatch failed");
    require(scheduler.finish_dispatch(process, {StepDisposition::waiting}).has_value(), "wait failed");
    require(scheduler.set_suspended(process, true).has_value(), "waiter suspend failed");
    require(scheduler.set_suspended(process, false).has_value(), "waiter resume failed");
    require(scheduler.set_suspended(process, false).has_value(), "resume not idempotent");
    require(scheduler.inspect(process)->state == ProcessState::waiting, "resume woke waiter");
    require(scheduler.begin_dispatch(process) == invalid, "waiting dispatch accepted");
}

// A normal return makes a terminal record until context destruction retires its identity.
void exit_context(Runtime &runtime, ProcessContext &context) {
    auto &scheduler = *runtime.scheduler();
    const auto process = context.identity();
    require(scheduler.register_process(context).has_value(), "exit registration failed");
    require(scheduler.begin_dispatch(process).has_value(), "exit dispatch failed");
    require(scheduler.finish_dispatch(process, {StepDisposition::exited, ExitReason::normal}).has_value(),
            "exit failed");
    require(scheduler.inspect(process)->exit_reason == ExitReason::normal, "exit reason lost");
    const auto invalid = std::unexpected(SchedulerError::invalid_transition);
    require(scheduler.begin_dispatch(process) == invalid, "exited process dispatched");
    require(scheduler.finish_dispatch(process, {}) == invalid, "exited process returned");
    require(scheduler.set_suspended(process, false) == invalid, "exited process resumed");
}

// Closing admission still permits an in-flight return and ordered runtime teardown.
void stop_dispatch(Runtime &runtime, ProcessContext &context) {
    auto &scheduler = *runtime.scheduler();
    const auto process = context.identity();
    auto *unregistered = runtime.create_context().value();
    require(scheduler.register_process(context).has_value(), "shutdown registration failed");
    require(scheduler.begin_dispatch(process).has_value(), "shutdown dispatch failed");
    scheduler.request_shutdown();
    scheduler.request_shutdown();
    require(scheduler.stopping(), "shutdown lost");
    require(scheduler.register_process(*unregistered) == std::unexpected(SchedulerError::stopped),
            "closed registration admitted");
    require(scheduler.begin_dispatch(process) == std::unexpected(SchedulerError::stopped), "stopped dispatch admitted");
    require(scheduler.set_suspended(process, false) == std::unexpected(SchedulerError::stopped),
            "stopped control admitted");
    require(scheduler.finish_dispatch(process, {StepDisposition::exited, ExitReason::runtime_shutdown}).has_value(),
            "shutdown return refused");
    require(runtime.destroy_context(unregistered) == Status::ok, "unregistered shutdown cleanup failed");
}

// Copies and callable pins survive their source runtime while context tokens become dead.
void workflow() {
    auto runtime = Runtime::start({2}).value();
    auto foreign = Runtime::start().value();
    invalid_options(*runtime);
    auto *source = runtime->create_context().value();
    auto *destination = runtime->create_context({sizeof(Word), sizeof(Word)}).value();
    auto *other = foreign->create_context().value();
    require(source->identity() != destination->identity() && source->identity() != other->identity(),
            "identity collision");
    require(&source->heap() != &destination->heap() && &source->mailbox() != &destination->mailbox(), "owners alias");
    require(runtime->destroy_context(other) == Status::wrong_owner, "foreign context destroyed");
    require(runtime->context_count() == 2 && foreign->context_count() == 1, "foreign rejection changed ownership");
    require(runtime->create_context() == std::unexpected(Status::resource_limit), "admission limit ignored");
    auto target = install(*runtime);
    require(!foreign->code_server()->find_module("consumer"), "module escaped runtime");
    scheduled_calls(*runtime, *destination, target);
    const auto retained = Term::from_word(*encode_integer(42))->copy_to(other->heap()).value();
    auto token = source->lifetime().lock();
    const auto identity_before = source->identity();
    exit_context(*runtime, *source);
    require(runtime->shutdown() == Status::busy && token->alive(), "busy shutdown changed liveness");
    require(runtime->destroy_context(source) == Status::ok && !token->alive(), "context teardown left live token");
    require(runtime->scheduler()->inspect(identity_before) == std::unexpected(SchedulerError::unknown_process),
            "stale process identity resolved");
    const auto replacement = runtime->create_context().value();
    require(replacement->identity() != identity_before, "identity reused");
    require(runtime->destroy_context(destination) == Status::ok, "waiting context teardown failed");
    require(runtime->scheduler()->process_count() == 0, "context teardown retained registration");
    auto survivor = replacement->lifetime().lock();
    stop_dispatch(*runtime, *replacement);
    runtime.reset();
    require(!survivor->alive(), "RAII teardown left live context");
    const std::array arguments{retained};
    require(target.call(*other, arguments)->integer_value() == 42, "pinned call lost code or copied term");
    const auto weak = other->lifetime();
    require(foreign->destroy_context(other) == Status::ok && weak.expired(), "lifetime storage retained");
    require(foreign->shutdown() == Status::ok && foreign->shutdown() == Status::ok, "shutdown not idempotent");
    require(foreign->scheduler() == nullptr, "stopped scheduler remained exposed");
    require(foreign->create_context() == std::unexpected(Status::stopped), "stopped runtime admitted context");
    require(retained.integer_value() == 42, "destination exit damaged immediate");
}

// Link solely through the generated-program target and repeat complete independent consumer lifecycles.
int main() {
    try {
        for (unsigned iteration = 0; iteration < 32; ++iteration) {
            workflow();
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected runtime consumer exception\n", stderr);
        return 1;
    }
}

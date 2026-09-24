#include "terms.hpp"
#include <erlang_aot/abi/builtins.hpp>
#include <erlang_aot/runtime/builtins.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <erlang_aot/runtime/scheduler.hpp>
#include <iostream>
#include <stdexcept>

namespace {
using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

// Retain behavioral checks in optimized builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct Reports {
    // Count owner reports and retain the last message for feature/operation checks.
    unsigned count = 0;
    std::string last;
};

// Copy one diagnostic while the service's formatting buffer is alive.
bool record(void *state, std::string_view message) {
    auto &reports = *static_cast<Reports *>(state);
    ++reports.count;
    reports.last = message;
    return true;
}

// Exercise delivery rejection through real service calls, not only the reporter utility.
bool reject(void *, std::string_view) { return false; }

// Exercise a throwing delivery callback without allowing exceptions through service boundaries.
bool throwing(void *, std::string_view) { throw std::runtime_error("sink failure"); }

// Every constructible factory call must fail once without manufacturing a value or root.
void check_factory(ProcessContext &context) {
    Reports reports;
    TermFactory factory(context, {&reports, record});
    const auto term = *Term::from_word(*encode_integer(3));
    const std::array results{factory.integer(3),
                             factory.integer_decimal("123"),
                             factory.floating(1.5),
                             factory.atom("name"),
                             factory.boolean(true),
                             factory.nil(),
                             factory.cons(term, term),
                             factory.list({}),
                             factory.tuple({}),
                             factory.map({}),
                             factory.binary({}),
                             factory.bitstring({}, 0),
                             factory.pid(context.identity()),
                             factory.make_reference(),
                             factory.external_function(term, term, 0)};
    for (const auto &result : results) {
        require(result == std::unexpected(TermError::not_implemented), "factory fabricated a term");
    }
    require(reports.count == results.size() && reports.last.starts_with("[term services] notimpl"),
            "factory report count/feature wrong");
    TermFactory failed(context, {nullptr, reject});
    require(failed.atom("name") == std::unexpected(TermError::diagnostic_failure), "factory sink error lost");
}

// Moving/expiring a factory must not retain or dereference destroyed context storage.
void check_factory_lifetime() {
    auto runtime = Runtime::start().value();
    auto *context = runtime->create_context().value();
    Reports reports;
    TermFactory original(*context, {&reports, record});
    TermFactory moved(std::move(original));
    const auto token = context->lifetime().lock();
    require(runtime->destroy_context(context) == Status::ok, "factory pinned process storage");
    require(!token->alive(), "context token was not invalidated");
    require(moved.nil() == std::unexpected(TermError::expired_context), "factory dereferenced expired owner");
    require(reports.count == 0 && runtime->shutdown() == Status::ok, "expired factory reported/retained resources");
}

// Heap validation stays silent; real deferred work reports once and leaves accounting unchanged.
void check_memory(ProcessContext &context) {
    Reports reports;
    const DiagnosticSink sink{&reports, record};
    auto &heap = context.heap();
    require(heap.allocate(0, sink) == std::unexpected(HeapError::invalid_size), "invalid allocation misclassified");
    require(reports.count == 0, "validation reached placeholder");
    require(heap.allocate(1, sink) == std::unexpected(HeapError::not_implemented), "allocation fabricated");
    require(reports.count == 1 && reports.last.starts_with("[allocation] notimpl"), "allocation report wrong");
    require(heap.collect(sink) == std::unexpected(HeapError::not_implemented), "collection fabricated");
    require(reports.count == 2 && reports.last.starts_with("[garbage collection] notimpl"), "GC report wrong");
    require(heap.allocate(1, {nullptr, reject}) == std::unexpected(HeapError::diagnostic_failure),
            "allocation sink failure lost");
    require(heap.collect({nullptr, throwing}) == std::unexpected(HeapError::diagnostic_failure),
            "collection sink exception escaped");
    require(heap.used_words() == 0 && heap.capacity_words() == 0, "placeholder changed accounting");
}

// Execution hooks must not consume admission, change state, or fabricate a cooperative return.
void check_execution(Runtime &runtime, ProcessContext &context) {
    Reports reports;
    const DiagnosticSink sink{&reports, record};
    auto &scheduler = *runtime.scheduler();
    const auto identity = context.identity();
    require(scheduler.execute(identity, sink) == std::unexpected(SchedulerError::unknown_process),
            "unknown process executed");
    require(scheduler.register_process(context).has_value(), "registration failed");
    require(scheduler.run(sink) == std::unexpected(SchedulerError::not_implemented), "worker started");
    require(reports.count == 1 && reports.last.starts_with("[scheduling] notimpl"), "worker report wrong");
    require(scheduler.execute(identity, sink) == std::unexpected(SchedulerError::not_implemented),
            "execution fabricated a return");
    require(reports.count == 2 && reports.last.starts_with("[process execution] notimpl"), "execution report wrong");
    require(scheduler.inspect(identity)->state == ProcessState::runnable, "execution mutated lifecycle");
    require(scheduler.run({nullptr, reject}) == std::unexpected(SchedulerError::diagnostic_failure),
            "worker sink error lost");
    require(scheduler.execute(identity, {nullptr, reject}) == std::unexpected(SchedulerError::diagnostic_failure),
            "execution sink error lost");
    scheduler.request_shutdown();
    require(scheduler.run(sink) == std::unexpected(SchedulerError::stopped), "stopped workers admitted");
    require(scheduler.execute(identity, sink) == std::unexpected(SchedulerError::stopped),
            "stopped execution admitted");
    require(reports.count == 2, "ordinary lifecycle failures reported deferred work");
}

// Self-send is still a signal operation; it must fail instead of silently delivering or dropping a message.
void check_send(ProcessContext &context) {
    Reports reports;
    const auto term = *Term::from_word(*encode_integer(1));
    require(context.send(context.identity(), term, {&reports, record}) ==
                std::unexpected(ProcessError::not_implemented),
            "send fabricated acceptance");
    require(reports.count == 1 && reports.last.starts_with("[message passing] notimpl"), "send report wrong");
    require(context.send(context.identity(), term, {nullptr, reject}) ==
                std::unexpected(ProcessError::diagnostic_failure),
            "send sink error lost");
}

// Publish a harmless linked module so unload rejection can be checked against real retained ownership.
std::weak_ptr<const CodeImage> install(CodeServer &server) {
    auto image = CodeImage::linked();
    require(server.load({"kept", image, std::make_unique<ModuleRegistry>()}).has_value(), "module fixture failed");
    return image;
}

// Deferred unload preserves publication and image lifetime, even when diagnostic delivery fails.
void check_unload(CodeServer &server) {
    Reports reports;
    const DiagnosticSink sink{&reports, record};
    const auto image = install(server);
    require(server.unload("unknown", sink) == std::unexpected(CodeError::module_not_found), "unknown unload accepted");
    require(reports.count == 0, "missing module reported as deferred");
    require(server.unload("kept", sink) == std::unexpected(CodeError::not_implemented), "module unloaded");
    require(reports.count == 1 && reports.last.starts_with("[dynamic modules] notimpl"), "unload report wrong");
    require(server.unload("kept", {nullptr, reject}) == std::unexpected(CodeError::diagnostic_failure),
            "unload sink error lost");
    require(server.find_module("kept").has_value() && !image.expired(), "failed unload released module");
}

// Exact name/module/arity matching is independent of runtime registry contents and term values.
void check_builtin_identity(ProcessContext &context) {
    for (const auto &entry : deferred_builtins) {
        require(is_deferred_builtin(entry), "catalog entry unrecognized");
        require(!is_deferred_builtin({entry.module, entry.function, 255}), "wrong arity recognized");
        require(!is_deferred_builtin({"other", entry.function, entry.arity}), "wrong module recognized");
    }
    Word output = 123;
    require(erlang_aot::abi::v1::dispatch_builtin(&context, "erlang", 6, "missing", 7, nullptr, 0, &output) ==
                Status::unknown_builtin,
            "unknown BIF mislabeled deferred");
    require(erlang_aot::abi::v1::dispatch_builtin(&context, "erlang", 6, "self", 4, &output, 1, &output) ==
                Status::unknown_builtin,
            "wrong BIF arity recognized");
    require(output == 123, "unknown BIF changed output");
}

// A checked native wrapper preserves a service owner's already-reported failure across the ABI bridge.
void report_nested(ProcessContext &context) {
    auto registry = std::make_unique<ModuleRegistry>();
    require(registry
                ->add("collect", 0,
                      [](ProcessContext &caller, std::span<const Term>) -> CallResult<Term> {
                          const auto result = caller.heap().collect();
                          const auto code = result.error() == HeapError::not_implemented
                                                ? CallError::not_implemented
                                                : CallError::diagnostic_failure;
                          return std::unexpected(CallFailure{.code = code, .reported = true});
                      })
                .has_value(),
            "nested service registration failed");
    require(context.code_server().load({"wrapper", CodeImage::linked(), std::move(registry)}).has_value(),
            "nested service publication failed");
    Word output = 123;
    require(erlang_aot::abi::v1::dispatch_builtin(&context, "wrapper", 7, "collect", 7, nullptr, 0, &output) ==
                Status::not_implemented,
            "nested service failure lost");
    require(output == 123, "nested service fabricated result");
}

// Term/memory subprocess modes reach only one report owner.
bool report_storage(std::string_view mode, ProcessContext &context) {
    if (mode == "term") {
        require(TermFactory(context).nil() == std::unexpected(TermError::not_implemented), "term status wrong");
    } else if (mode == "allocate") {
        require(context.heap().allocate(1) == std::unexpected(HeapError::not_implemented), "allocation status wrong");
    } else if (mode == "collect") {
        require(context.heap().collect() == std::unexpected(HeapError::not_implemented), "collection status wrong");
    } else {
        return false;
    }
    return true;
}

// Process/scheduler subprocess modes never execute user code or mutate dispatch state.
bool report_execution(std::string_view mode, Runtime &runtime, ProcessContext &context) {
    auto &scheduler = *runtime.scheduler();
    if (mode == "send") {
        require(context.send(context.identity(), *Term::from_word(*encode_integer(1))) ==
                    std::unexpected(ProcessError::not_implemented),
                "send status wrong");
    } else if (mode == "run") {
        require(scheduler.run() == std::unexpected(SchedulerError::not_implemented), "worker status wrong");
    } else if (mode == "execute") {
        require(scheduler.execute(context.identity()) == std::unexpected(SchedulerError::not_implemented),
                "execution status wrong");
    } else {
        return false;
    }
    return true;
}

// Select one actual service boundary for subprocess stderr-count assertions.
void report_one(std::string_view mode, Runtime &runtime, ProcessContext &context) {
    require(runtime.scheduler()->register_process(context).has_value(), "registration failed");
    if (report_storage(mode, context) || report_execution(mode, runtime, context)) {
        return;
    }
    if (mode == "unload") {
        install(context.code_server());
        require(context.code_server().unload("kept") == std::unexpected(CodeError::not_implemented),
                "unload status wrong");
    } else if (mode == "builtin") {
        Word output = 123;
        require(erlang_aot::abi::v1::dispatch_builtin(&context, "erlang", 6, "self", 4, nullptr, 0, &output) ==
                    Status::not_implemented,
                "known BIF status wrong");
        require(output == 123, "known deferred BIF wrote a result");
    } else if (mode == "nested") {
        report_nested(context);
    } else {
        require(mode == "quiet", "unknown test mode");
    }
}
} // namespace

// Exercise real owners and prove teardown after every explicit service failure.
int main(int argc, char **argv) {
    try {
        auto runtime = Runtime::start().value();
        auto *context = runtime->create_context().value();
        if (argc == 2) {
            report_one(argv[1], *runtime, *context);
        } else {
            check_factory(*context);
            check_factory_lifetime();
            check_memory(*context);
            check_send(*context);
            check_execution(*runtime, *context);
            check_unload(context->code_server());
            check_builtin_identity(*context);
        }
        auto image = context->code_server().find_module("kept");
        const std::weak_ptr<const LoadedModule> weak = image ? *image : std::shared_ptr<const LoadedModule>{};
        image = std::unexpected(CodeError::module_not_found);
        require(runtime->destroy_context(context) == Status::ok, "placeholder prevented context cleanup");
        require(runtime->shutdown() == Status::ok && weak.expired(), "placeholder retained runtime resources");
        return argc == 2 && std::string_view(argv[1]) != "quiet" ? 1 : 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

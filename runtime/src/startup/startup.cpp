#include "startup.hpp"
#include "../process/exceptions.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <erlang_aot/abi/startup.hpp>
#include <erlang_aot/runtime/modules.hpp>
#include <erlang_aot/runtime/output.hpp>
#include <span>

namespace erlang_aot::runtime::detail {
namespace {
using abi::v1::StartupDescriptor;
using abi::v1::Status;

// Readable names of infrastructure statuses for runtime-failure messages.
std::string status_name(Status status) {
    static constexpr std::array<std::string_view, 14> names{"ok",
                                                            "not_implemented",
                                                            "invalid_argument",
                                                            "diagnostic_failure",
                                                            "out_of_memory",
                                                            "busy",
                                                            "wrong_owner",
                                                            "resource_limit",
                                                            "stopped",
                                                            "abi_mismatch",
                                                            "internal_error",
                                                            "unknown_builtin",
                                                            "erlang_error",
                                                            "output_failure"};
    const auto index = static_cast<std::size_t>(status);
    return index < names.size() ? std::string(names[index]) : "status " + std::to_string(index);
}

// Print one line on stderr after flushing program output, so both streams keep their relative order.
void report(std::string_view prefix, std::string_view text) noexcept {
    std::fflush(stdout);
    std::fwrite(prefix.data(), 1, prefix.size(), stderr);
    std::fwrite(text.data(), 1, text.size(), stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

// Report a failure outside Erlang code (startup, registration, infrastructure) and select its exit status.
// Writing never allocates, so allocation-failure handlers can use it too.
int runtime_failure(std::string_view what) noexcept {
    report("erlangaot: runtime failure: ", what);
    return abi::v1::exit_runtime_failure;
}

// Reject startup and module descriptors of another ABI revision or width before anything is registered.
bool compatible(const StartupDescriptor &startup) {
    constexpr auto bits = sizeof(abi::v1::TermWord) * 8;
    if (startup.abi_version != abi::v1::version || startup.term_bits != bits ||
        (startup.flags & ~abi::v1::startup_escript) != 0 || (startup.module_count != 0 && !startup.modules) ||
        !startup.entry_module || !startup.entry_function) {
        return false;
    }
    return std::ranges::all_of(std::span(startup.modules, startup.module_count), [](const auto *module) {
        return module && module->abi_version == abi::v1::version && module->term_bits == bits;
    });
}

// Map a failed entry call to its report and exit status: halt request, uncaught exception or runtime failure.
int failed_entry(const CallFailure &failure, bool escript) {
    if (failure.code == CallError::halted) {
        if (failure.value) {
            report("", slogan_text(*failure.value).value_or(""));
        }
        return failure.halt_status.value_or(abi::v1::exit_uncaught);
    }
    if (failure.code != CallError::erlang_exception) {
        return runtime_failure("entry call failed: " + status_name(failure.status.value_or(Status::internal_error)));
    }
    const auto reason = exception_reason(failure);
    if (!reason) {
        return runtime_failure("cannot format the uncaught exception");
    }
    const auto prefix = std::string(escript ? "escript: exception " : "uncaught exception ") +
                        std::string(exception_class(failure)) + ": ";
    report(prefix, *reason);
    return escript ? abi::v1::exit_escript_uncaught : abi::v1::exit_uncaught;
}

// Build argv, resolve the arity-1 entry and run it to completion in the first context.
int call_entry(ProcessContext &context, const StartupDescriptor &startup, int argc, char **argv) {
    const auto arguments = program_arguments(context, argc, argv);
    if (!arguments) {
        return runtime_failure("cannot build the argument list");
    }
    const std::string_view module(startup.entry_module, startup.entry_module_size);
    const std::string_view function(startup.entry_function, startup.entry_function_size);
    const auto entry = context.code_server().resolve({module, function, 1});
    if (!entry) {
        return runtime_failure("entry function " + std::string(module) + ":" + std::string(function) +
                               "/1 is not registered");
    }
    const auto result = entry->call(context, std::array{*arguments});
    return result ? 0 : failed_entry(result.error(), (startup.flags & abi::v1::startup_escript) != 0);
}

// Register every module before the entry runs; a failure discards the whole runtime before entry.
int run_program(Runtime &runtime, const StartupDescriptor &startup, int argc, char **argv) {
    for (const auto *module : std::span(startup.modules, startup.module_count)) {
        if (!register_module(runtime, *module)) {
            return runtime_failure("cannot register module " + std::string(module->name, module->name_size));
        }
    }
    const auto context = runtime.create_context();
    if (!context) {
        return runtime_failure("cannot create the entry process: " + status_name(context.error()));
    }
    const int status = call_entry(**context, startup, argc, argv);
    const auto destroyed = runtime.destroy_context(*context);
    return destroyed == Status::ok ? status : runtime_failure("entry process teardown: " + status_name(destroyed));
}

// Own the runtime for one program run and shut it down in order on every path.
int run(const StartupDescriptor &startup, int argc, char **argv) {
    if (!compatible(startup)) {
        return runtime_failure("startup ABI mismatch (rebuild objects and runtime together)");
    }
    auto runtime = Runtime::start();
    if (!runtime) {
        return runtime_failure("cannot start the runtime: " + status_name(runtime.error()));
    }
    const int status = run_program(**runtime, startup, argc, argv);
    const auto stopped = (*runtime)->shutdown();
    std::fflush(stdout);
    return stopped == Status::ok ? status : runtime_failure("shutdown: " + status_name(stopped));
}
} // namespace

TermResult<std::string> exception_reason(const CallFailure &failure) {
    const auto name = failure.reason ? error_name(*failure.reason) : std::string_view{};
    // Raised reasons (error/exit/throw) are the whole payload term.
    if (name.empty() && failure.reason && failure.value) {
        return format_term(*failure.value, TermStyle::write);
    }
    if (name.empty()) {
        return std::unexpected(TermError::invalid_argument);
    }
    if (!failure.value) {
        return std::string(name);
    }
    return format_term(*failure.value, TermStyle::write).transform([&](const std::string &value) {
        return "{" + std::string(name) + "," + value + "}";
    });
}
} // namespace erlang_aot::runtime::detail

int erlang_aot_main_v1(int argc, char **argv, const void *startup) noexcept {
    using namespace erlang_aot;
    if (!startup) {
        return runtime::detail::runtime_failure("missing startup descriptor");
    }
    try {
        return runtime::detail::run(*static_cast<const abi::v1::StartupDescriptor *>(startup), argc, argv);
    } catch (const std::bad_alloc &) {
        return runtime::detail::runtime_failure("out of memory");
    } catch (...) {
        return runtime::detail::runtime_failure("internal error");
    }
}

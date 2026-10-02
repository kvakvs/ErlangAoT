#include <array>
#include <cstdio>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/immediate_services.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/modules.hpp>
#include <stdexcept>

using namespace erlang_aot;
using namespace erlang_aot::runtime;
extern abi::v1::GeneratedRegistration register_answer asm("eav1_736572766963655f616e73776572__0.register");
extern abi::v1::GeneratedRegistration register_client asm("eav1_736572766963655f636c69656e74__0.register");
extern std::uint8_t injected(void *, std::uint8_t, Word, Word, Word *) noexcept asm("step7_service");

namespace {
// Select faults after source-generated code has entered the real invocation scope.
abi::v1::Status fault = abi::v1::Status::ok;
// Count service entry to prove the fault happened in generated code rather than host admission.
unsigned calls = 0;
// Target only the reached predicate when proving that lazy branches skip faults and strict branches do not.
bool predicate_only = false;
unsigned predicate_calls = 0;

// Keep ownership, status and recovery assertions enabled in optimized consumers.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Exact infrastructure statuses must survive both head/body services and nested generated calls.
void failures(ProcessContext &context, std::string_view module, std::string_view name) {
    const auto entry = context.code_server().resolve({module, name, 1}).value();
    const std::array arguments{Term::from_word(encode_integer(42).value()).value()};
    for (const auto status :
         {abi::v1::Status::out_of_memory, abi::v1::Status::resource_limit, abi::v1::Status::wrong_owner,
          abi::v1::Status::not_implemented, abi::v1::Status::internal_error}) {
        fault = status;
        calls = 0;
        const auto result = entry.call(context, arguments);
        require(!result && result.error().code == CallError::runtime_failure && result.error().status == status,
                "service failure became semantic rejection or success");
        require(calls == 1 && !context.generated_calls().failure(), "service did not run or stale channel");
        fault = abi::v1::Status::ok;
        require(entry.call(context, arguments).has_value(), "service failure poisoned retry");
    }
}

// A rejected source head must reach function_clause before entering any guard service.
void head_mismatch(ProcessContext &context) {
    const std::array arguments{Term::from_word(encode_integer(42).value()).value()};
    fault = abi::v1::Status::out_of_memory;
    calls = 0;
    const auto result =
        context.code_server().resolve({"service_answer", "head_mismatch", 1}).value().call(context, arguments);
    require(!result && result.error().reason == abi::v1::ErrorReason::function_clause && calls == 0,
            "failing head evaluated its guard");
    fault = abi::v1::Status::ok;
}

// Generic builtin registrations cannot replace a guard identity authorized by the compiler.
void registration(ProcessContext &context) {
    auto registry = std::make_unique<ModuleRegistry>();
    auto entries = std::make_shared<unsigned>(0);
    require(registry
                ->add("is_integer", 1,
                      [entries](ProcessContext &, std::span<const Term> arguments) {
                          ++*entries;
                          return CallResult<Term>(arguments.front());
                      })
                .has_value(),
            "builtin seam registration failed");
    require(context.code_server().load({"erlang", CodeImage::linked(), std::move(registry)}).has_value(),
            "builtin seam publication failed");
    const std::array arguments{Term::from_word(encode_integer(42).value()).value()};
    const auto result = context.code_server().resolve({"service_answer", "head", 1}).value().call(context, arguments);
    require(result.has_value() && *entries == 0, "registered builtin replaced guard service");
}

// Infrastructure failure cannot take nested orelse or semicolon recovery, and skipped predicates never run.
void boolean_failures(ProcessContext &context) {
    predicate_only = true;
    const std::array arguments{Term::from_word(encode_integer(42).value()).value()};
    for (const auto name : {"reached", "strict"}) {
        failures(context, "service_answer", name);
    }
    fault = abi::v1::Status::out_of_memory;
    predicate_calls = 0;
    const auto result =
        context.code_server().resolve({"service_answer", "skipped", 1}).value().call(context, arguments);
    require(result && result->atom_spelling() == "true" && predicate_calls == 0,
            "skipped right predicate ran or did not preserve term result");
    fault = abi::v1::Status::ok;
    predicate_only = false;
}

// Count real services to prove a matched RHS runs once and a failed match skips later body work.
void body_matches(ProcessContext &context) {
    const std::array arguments{Term::from_word(encode_integer(42).value()).value()};
    calls = 0;
    const auto once =
        context.code_server().resolve({"service_answer", "match_once", 1}).value().call(context, arguments);
    require(once && once->atom_spelling() == "true" && calls == 1, "matched RHS was not evaluated exactly once");
    calls = 0;
    const auto failed =
        context.code_server().resolve({"service_answer", "match_stop", 1}).value().call(context, arguments);
    require(!failed && failed.error().reason == abi::v1::ErrorReason::badmatch && calls == 1,
            "body mismatch ran later services or lost its reason");
    require(failed.error().value && failed.error().value->atom_spelling() == "true", "badmatch lost the RHS atom");
    failures(context, "service_answer", "match_once");
}

// Guard argument errors are channel-free; malformed/foreign service words are infrastructure failures.
void checked_arguments(ProcessContext &context) {
    auto other = Runtime::start().value();
    auto &foreign = *other->create_context().value();
    const auto atom = foreign.atom_storage().intern("foreign").value();
    for (Word value : {Word{0}, atom.word()}) {
        GeneratedInvocation invocation(context.generated_calls());
        Word output = 123;
        require(erlang_aot_immediate_v1(&context, static_cast<std::uint8_t>(abi::v1::ImmediateOperation::is_atom),
                                        value, 0, &output) == 2,
                "malformed/foreign service input admitted");
        require(output == 123 && context.generated_calls().failure(), "failure published output or lost channel");
    }
    GeneratedInvocation invocation(context.generated_calls());
    Word output = 123;
    require(erlang_aot_immediate_v1(&context, static_cast<std::uint8_t>(abi::v1::ImmediateOperation::hd),
                                    abi::v1::empty_list, 0, &output) == 1,
            "semantic badarg lost");
    require(output == 123 && !context.generated_calls().failure(), "semantic rejection polluted channel");
}
} // namespace

// This native seam changes only the service outcome and deliberately leaves success output untouched on faults.
std::uint8_t injected(void *context, std::uint8_t operation, Word left, Word right, Word *output) noexcept {
    ++calls;
    const bool predicate = operation == static_cast<std::uint8_t>(abi::v1::ImmediateOperation::is_integer);
    predicate_calls += predicate;
    if (fault != abi::v1::Status::ok && (!predicate_only || predicate)) {
        static_cast<ProcessContext *>(context)->generated_calls().fail_service(fault);
        return 2;
    }
    return erlang_aot_immediate_v1(context, operation, left, right, output);
}

int main() {
    try {
        auto runtime = Runtime::start().value();
        require(register_answer(runtime.get()) == 0 && register_client(runtime.get()) == 0,
                "service registration failed");
        auto &context = *runtime->create_context().value();
        failures(context, "service_answer", "head");
        failures(context, "service_answer", "fallback");
        failures(context, "service_answer", "body");
        failures(context, "service_client", "nested");
        head_mismatch(context);
        registration(context);
        boolean_failures(context);
        body_matches(context);
        checked_arguments(context);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

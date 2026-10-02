#include "match_wire.hpp"
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/modules.hpp>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

extern erlang_aot::abi::v1::GeneratedRegistration register_answer asm("eav1_616e73776572__0.register");
extern erlang_aot::abi::v1::GeneratedRegistration register_client asm("eav1_636c69656e74__0.register");

namespace {
using namespace erlang_aot::runtime;

// Retain readable test failures in optimized native builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Decode owned terms through the bounded value transport and reject trailing input.
Term term(ProcessContext &context, const std::string &token) {
    std::string_view input(token);
    const auto result = wire::read(context, input);
    require(input.empty(), "trailing argument input");
    return result;
}

// End each successful or retained-error result with exactly one transport newline.
void print_value(const Term &value) {
    wire::write(value);
    std::cout << '\n';
}

// Compare OTP-visible values and structured error reasons without depending on stack formatting.
void print(const CallResult<Term> &result) {
    if (result) {
        print_value(*result);
        return;
    }
    require(result.error().code == CallError::erlang_exception, "unexpected infrastructure failure");
    const auto reason = result.error().reason;
    if (reason == erlang_aot::abi::v1::ErrorReason::badarg_value ||
        reason == erlang_aot::abi::v1::ErrorReason::badmatch || reason == erlang_aot::abi::v1::ErrorReason::badmap ||
        reason == erlang_aot::abi::v1::ErrorReason::badkey || reason == erlang_aot::abi::v1::ErrorReason::badrecord) {
        require(result.error().value.has_value(), "missing error payload");
        const std::map<erlang_aot::abi::v1::ErrorReason, std::string_view> names{
            {erlang_aot::abi::v1::ErrorReason::badmatch, "badmatch"},
            {erlang_aot::abi::v1::ErrorReason::badarg_value, "badarg_value"},
            {erlang_aot::abi::v1::ErrorReason::badmap, "badmap"},
            {erlang_aot::abi::v1::ErrorReason::badkey, "badkey"},
            {erlang_aot::abi::v1::ErrorReason::badrecord, "badrecord"}};
        std::cout << "error:" << names.at(*reason) << ':';
        print_value(*result.error().value);
        return;
    }
    require(reason == erlang_aot::abi::v1::ErrorReason::function_clause ||
                reason == erlang_aot::abi::v1::ErrorReason::badarg ||
                reason == erlang_aot::abi::v1::ErrorReason::badarith,
            "unexpected Erlang reason");
    if (reason == erlang_aot::abi::v1::ErrorReason::badarith) {
        std::cout << "error:badarith\n";
        return;
    }
    std::cout << (reason == erlang_aot::abi::v1::ErrorReason::badarg ? "error:badarg\n" : "error:function_clause\n");
}

// Runtime-only invalid word/foreign ownership checks cannot be expressed as legal Erlang source.
void equality_failures(ProcessContext &context) {
    auto other = Runtime::start().value();
    auto &foreign = *other->create_context().value();
    const auto atom = foreign.atom_storage().intern("owned").value();
    for (const auto invalid : {Word{0}, Word{0x6b}, atom.word()}) {
        GeneratedInvocation scope(context.generated_calls());
        require(erlang_aot_exact_v1(&context, invalid, invalid) == 2, "invalid equality fabricated success");
        require(context.generated_calls().failure().has_value(), "invalid equality lost failure");
    }
    GeneratedInvocation scope(context.generated_calls());
    require(erlang_aot_exact_v1(&context, erlang_aot::abi::v1::empty_list, erlang_aot::abi::v1::empty_tuple) == 0,
            "equality did not recover");
}

// Invoke each complete source helper through its registered generated ABI entry.
void calls(ProcessContext &context) {
    std::vector<std::pair<Term, std::string>> retained;
    std::string module;
    std::string function;
    std::size_t arity = 0;
    while (std::cin >> module >> function >> arity) {
        require(arity <= 255, "call arity limit");
        std::vector<Term> arguments;
        for (std::size_t i = 0; i < arity; ++i) {
            std::string token;
            require(static_cast<bool>(std::cin >> token), "missing argument");
            arguments.push_back(term(context, token));
        }
        const auto result = context.code_server().resolve({module, function, arity}).value().call(context, arguments);
        print(result);
        if (result && (function == "first" || function == "id")) {
            require(result->word() == arguments.front().word(), "projection reconstructed a matched term");
        }
        const auto value = result ? std::optional<Term>{*result} : result.error().value;
        if (value && retained.size() < 128 &&
            (value->is_cons() || value->kind() == TermKind::tuple || value->is_float() || value->is_map())) {
            std::ostringstream text;
            wire::write(*value, text);
            retained.emplace_back(*value, text.str());
        }
        require(!context.generated_calls().failure(), "stale failure after invocation");
        require(context.roots().depth() == 0 && context.roots().words() == 0, "generated call leaked root frames");
    }
    require(std::cin.eof(), "invalid calls stream");
    for (const auto &[value, expected] : retained) {
        std::ostringstream text;
        wire::write(value, text);
        require(text.str() == expected, "later calls changed retained result/error graph");
    }
}
} // namespace

// The separate consumer has no LLVM dependency; it also proves recovery and explicit teardown.
int main() {
    try {
        auto runtime = Runtime::start().value();
        require(register_answer(runtime.get()) == 0 && register_client(runtime.get()) == 0, "registration failed");
        auto *context = runtime->create_context().value();
        equality_failures(*context);
        calls(*context);
        require(runtime->destroy_context(context) == erlang_aot::abi::v1::Status::ok, "context teardown failed");
        require(runtime->shutdown() == erlang_aot::abi::v1::Status::ok, "runtime teardown failed");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

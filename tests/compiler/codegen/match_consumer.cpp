#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/atoms.hpp>
#include <erlang_aot/runtime/modules.hpp>
#include <iostream>
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

// Decode stable hex spellings without admitting fabricated atom IDs.
std::string unhex(std::string_view input) {
    std::string output;
    require(input.size() % 2 == 0, "invalid hex spelling");
    for (std::size_t i = 0; i < input.size(); i += 2) {
        output.push_back(static_cast<char>(std::stoul(std::string(input.substr(i, 2)), nullptr, 16)));
    }
    return output;
}

// Construct every argument through real runtime admission/atom ownership.
Term term(ProcessContext &context, const std::string &token) {
    if (token == "nil" || token == "tuple") {
        return Term::from_word(token == "nil" ? erlang_aot::abi::v1::empty_list : erlang_aot::abi::v1::empty_tuple)
            .value();
    }
    if (token.starts_with('a')) {
        return context.atom_storage().intern(unhex(std::string_view(token).substr(1))).value();
    }
    require(token.starts_with('i'), "unknown argument token");
    return Term::from_word(encode_integer(std::stoll(token.substr(1))).value()).value();
}

// Compare OTP-visible values and error reasons, never unstable raw IDs or stacks.
void print(const CallResult<Term> &result) {
    if (!result) {
        require(result.error().code == CallError::erlang_exception, "unexpected infrastructure failure");
        require(result.error().reason == erlang_aot::abi::v1::ErrorReason::function_clause, "unexpected Erlang reason");
        std::cout << "error:function_clause\n";
        return;
    }
    const auto &value = *result;
    if (value.is_atom()) {
        static constexpr std::string_view digits = "0123456789abcdef";
        std::cout << 'a';
        for (unsigned char byte : value.atom_utf8().value()) {
            std::cout << digits[byte >> 4] << digits[byte & 15];
        }
    } else if (value.kind() == TermKind::empty_list) {
        std::cout << "nil";
    } else if (value.kind() == TermKind::empty_tuple) {
        std::cout << "tuple";
    } else {
        std::cout << 'i' << value.integer_value().value();
    }
    std::cout << '\n';
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
        print(context.code_server().resolve({module, function, arity}).value().call(context, arguments));
        require(!context.generated_calls().failure(), "stale failure after invocation");
    }
    require(std::cin.eof(), "invalid calls stream");
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

#include <array>
#include <erlang_aot/runtime/modules.hpp>
#include <iostream>
#include <process_heap.hpp>
#include <string>
#include <vector>

extern erlang_aot::abi::v1::GeneratedRegistration register_answer asm("eav1_616e73776572__0.register");
extern erlang_aot::abi::v1::GeneratedRegistration register_client asm("eav1_636c69656e74__0.register");

namespace {
using namespace erlang_aot::runtime;

// Reject incompatible descriptors before reading metadata or publishing callable entries.
bool rejects_incompatible(Runtime &runtime) {
    erlang_aot::abi::v1::ModuleDescriptor bad{};
    bad.abi_version = erlang_aot::abi::v1::version + 1;
    bad.term_bits = sizeof(Word) * 8;
    if (erlang_aot_register_module_v3(&runtime, &bad) == 0) {
        return false;
    }
    bad.abi_version = erlang_aot::abi::v1::version;
    bad.term_bits = sizeof(Word) == 8 ? 32 : 64;
    return erlang_aot_register_module_v3(&runtime, &bad) != 0;
}

// Exercise identity with every supported immediate family and native integer endpoints.
bool identity_boundaries(Runtime &runtime, ProcessContext &context) {
    const auto identity = runtime.code_server()->resolve({"answer", "identity", 1}).value();
    const auto minimum = -(std::int64_t{1} << (sizeof(Word) * 8 - 5));
    for (const auto word :
         {encode_integer(minimum).value(), encode_integer(-minimum - 1).value(), Word{0x2b}, Word{0x3b}}) {
        const std::array args{Term::from_word(word).value()};
        const auto result = identity.call(context, args);
        if (!result || result->word() != word) {
            return false;
        }
    }
    return true;
}

// Read bounded integer calls and print decoded results for independent golden/OTP comparison.
bool calls(Runtime &runtime, ProcessContext &context) {
    std::string module;
    std::string function;
    std::size_t arity = 0;
    while (std::cin >> module >> function >> arity) {
        if (arity > 255) {
            return false;
        }
        std::vector<Term> arguments;
        for (std::size_t i = 0; i < arity; ++i) {
            std::int64_t value = 0;
            if (!(std::cin >> value)) {
                return false;
            }
            arguments.push_back(Term::from_word(encode_integer(value).value()).value());
        }
        const auto entry = runtime.code_server()->resolve({module, function, arity});
        if (!entry) {
            return false;
        }
        const auto result = entry->call(context, arguments);
        if (!result || !result->integer_value()) {
            return false;
        }
        std::cout << *result->integer_value() << '\n';
    }
    return std::cin.eof();
}

// Preserve generated calls and explicit teardown even when a reached service returns deferred failure.
int execute(Runtime &runtime, bool deferred) {
    auto *context = runtime.create_context().value();
    if (deferred && (context->heap().allocate(1) != std::unexpected(HeapError::not_implemented) ||
                     context->heap().used_words() != 0 || context->heap().capacity_words() != 0)) {
        return 4;
    }
    const bool success = identity_boundaries(runtime, *context) && calls(runtime, *context);
    if (runtime.destroy_context(context) != erlang_aot::abi::v1::Status::ok ||
        runtime.shutdown() != erlang_aot::abi::v1::Status::ok) {
        return 2;
    }
    return success ? static_cast<int>(deferred) : 3;
}
} // namespace

// Own explicit startup and registration around real CLI-generated objects.
int main(int argc, char **argv) {
    using namespace erlang_aot::runtime;
    auto runtime = Runtime::start().value();
    if (!rejects_incompatible(*runtime) || register_answer(runtime.get()) != 0 || register_client(runtime.get()) != 0) {
        return 1;
    }
    return execute(*runtime, argc == 2 && std::string_view(argv[1]) == "--deferred-allocation");
}

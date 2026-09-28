#include <array>
#include <erlang_aot/abi/modules.hpp>
#include <erlang_aot/runtime/modules.hpp>

// Bind native C++ declarations to the project-generated symbol names without a C linkage wrapper.
extern erlang_aot::abi::v1::GeneratedRegistration register_answer asm("eav1_616e73776572__0.register");
extern erlang_aot::abi::v1::GeneratedRegistration register_client asm("eav1_636c69656e74__0.register");

// Require explicit registration, then execute separately linked source modules through real runtime handles.
int main() {
    using namespace erlang_aot::runtime;
    auto runtime = Runtime::start().value();
    auto *server = runtime->code_server();
    if (server->resolve({"client", "value", 0}) || register_answer(runtime.get()) != 0 ||
        register_client(runtime.get()) != 0 || register_answer(runtime.get()) == 0) {
        return 1;
    }
    auto *context = runtime->create_context().value();
    const auto value = server->resolve({"client", "value", 0}).value().call(*context, {});
    if (!value || value->integer_value() != 42) {
        return 2;
    }
    const auto identity = server->resolve({"answer", "identity", 1}).value();
    const std::array arguments{Term::from_word(0x3b).value()};
    const auto result = identity.call(*context, arguments);
    if (!result || result->word() != arguments.front().word()) {
        return 3;
    }
    return 0;
}

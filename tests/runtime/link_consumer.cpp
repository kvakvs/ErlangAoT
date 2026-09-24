#include <erlang_aot/runtime/runtime.hpp>
#include <type_traits>

using erlang_aot::abi::v1::GeneratedFunction;
using erlang_aot::abi::v1::Status;
using erlang_aot::abi::v1::TermWord;
using erlang_aot::runtime::ProcessContext;
using erlang_aot::runtime::Runtime;

static_assert(std::is_same_v<erlang_aot::abi::v1::Context, ProcessContext>);
static_assert(std::is_same_v<std::underlying_type_t<Status>, std::uint8_t>);
static_assert(!std::is_convertible_v<Status, std::uint32_t>);

// A generated-entry-shaped project function borrows the real context through the forward-declared ABI type.
TermWord identity(ProcessContext *context, const TermWord *arguments) { return context == nullptr ? 0 : arguments[0]; }

// Link solely through the generated-program target; RAII also cleans up every early failure.
int main() {
    auto runtime = Runtime::start();
    if (!runtime) {
        return 1;
    }
    auto context = (*runtime)->create_context();
    if (!context) {
        return 2;
    }
    GeneratedFunction *entry = identity;
    const TermWord argument = 0x2af;
    const bool matched = entry(*context, &argument) == argument;
    const auto destroyed = (*runtime)->destroy_context(*context);
    const auto stopped = (*runtime)->shutdown();
    return matched && destroyed == Status::ok && stopped == Status::ok ? 0 : 3;
}

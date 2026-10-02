#include <array>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

namespace {
// These lifecycle checks remain active in optimized native consumers.
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

// Nested results transfer before pop, and a host scope preserves an enclosing generated caller's roots.
void transfers(ProcessContext &context) {
    GeneratedInvocation invocation(context.generated_calls());
    auto &roots = context.roots();
    auto *outer = roots.enter(2);
    require(outer && outer[0] == 0 && outer[1] == 0, "roots were not zero initialized");
    outer[0] = encode_integer(11).value();
    const auto result = encode_integer(22).value();
    {
        RootInvocation nested(roots);
        auto *inner = roots.enter(3);
        require(inner && roots.words() == 5 && roots.depth() == 2, "nested accounting failed");
        require(roots.leave(inner, result) == Status::ok && roots.contains(result), "result lost during pop");
        require(roots.contains(outer[0]), "callee changed caller root");
        require(roots.enter(1) != nullptr, "exception fixture entry failed");
    }
    require(roots.depth() == 1 && roots.words() == 2, "nested exception cleanup removed caller roots");
    require(roots.leave(outer, result) == Status::ok && roots.contains(result), "host handoff lost result");
    roots.restore(0);
    require(roots.words() == 0 && roots.depth() == 0 && !roots.contains(result), "host cleanup retained roots");
}

// Capacity and order faults remain infrastructure failures and never discard older roots or first failures.
void limits(ProcessContext &context) {
    for (const auto options : {RootOptions{2, 2}, RootOptions{20, 1}}) {
        GeneratedInvocation invocation(context.generated_calls());
        GeneratedRoots roots(context, options);
        auto *first = roots.enter(2);
        require(first && !roots.enter(1), "root capacity ignored");
        require(context.generated_calls().failure()->status == Status::resource_limit, "root failure lost status");
        require(roots.words() == 2 && roots.depth() == 1, "failed entry published partial roots");
        require(roots.leave(first, 0) == Status::ok, "error cleanup failed");
    }
    GeneratedInvocation invocation(context.generated_calls());
    auto &roots = context.roots();
    auto *first = roots.enter(1);
    Word forged = 0;
    require(roots.leave(&forged, 0) == Status::internal_error && roots.depth() == 1, "out-of-order release accepted");
    require(roots.leave(first, 0) == Status::ok, "first-failure root cleanup failed");
    roots.restore(0);
}

// Null/unscoped calls reject before allocating or reading a result representation.
void boundaries(ProcessContext &context) {
    require(!context.roots().enter(1), "unscoped roots admitted");
    require(!erlang_aot_roots_enter_v4(nullptr, 1), "null context entered roots");
    require(erlang_aot_roots_leave_v4(nullptr, nullptr, 0) == static_cast<std::uint8_t>(Status::invalid_argument),
            "null leave admitted");
    GeneratedInvocation invocation(context.generated_calls());
    require(!context.roots().enter(std::numeric_limits<std::size_t>::max()), "overflow root count admitted");
    require(context.generated_calls().failure()->status == Status::resource_limit, "overflow status lost");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        auto &context = *runtime->create_context().value();
        boundaries(context);
        transfers(context);
        limits(context);
        transfers(context);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

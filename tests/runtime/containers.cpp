#include "terms.hpp"
#include <array>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot::runtime;
using erlang_aot::abi::v1::Status;

namespace {
// Public host invariants complement source execution for ownership and forged-word cases.
void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

// Keep independently constructed cells equal by value across stable backing growth.
Term values(ProcessContext &context) {
    TermFactory factory(context);
    const auto number = Term::from_word(encode_integer(42).value()).value();
    const auto atom = factory.atom("nested").value();
    const std::array elements{number, atom, factory.nil().value()};
    const auto proper = factory.list(elements).value();
    const auto improper = factory.list(elements, atom).value();
    require(proper.list_length() == 3 && proper.head()->integer_value() == 42, "proper list access failed");
    require(improper.list_length() == std::unexpected(TermError::improper_list), "improper tail accepted");
    require(improper.is_list() && improper.is_proper_list() == false, "list category confused with properness");
    const std::array nested{proper, improper, atom};
    const auto result = factory.tuple(nested).value();
    require(result.tuple_size() == 3 && result.tuple_element(1)->exactly_equal(improper) == true,
            "nested tuple access changed ownership");
    const auto separate =
        factory.tuple(std::array{factory.list(elements).value(), factory.list(elements, atom).value(), atom}).value();
    require(result.word() != separate.word() && result.exactly_equal(separate) == true,
            "pointer equality used for tuples");
    for (unsigned i = 0; i < 100; ++i) {
        require(factory.tuple(elements).has_value(), "heap growth failed");
    }
    require(result.tuple_element(0)->tail()->head()->atom_spelling() == "nested", "growth damaged retained extraction");
    require(result.tuple_element(3) == std::unexpected(TermError::out_of_range), "tuple bound not checked");
    require(number.head() == std::unexpected(TermError::wrong_type), "wrong shape was dereferenced");
    return result;
}

// A badmatch owns the graph before scopes clear, independently of later allocations and calls.
void failure_payload(ProcessContext &context, const Term &value) {
    std::optional<CallFailure> retained;
    {
        GeneratedInvocation call(context.generated_calls());
        require(erlang_aot_raise_v2(&context, erlang_aot::abi::v1::ErrorReason::badmatch, value.word()) == 0,
                "compound error rejected");
        retained = context.generated_calls().failure();
    }
    require(retained && retained->value && !context.generated_calls().failure(),
            "error payload/channel ownership failed");
    values(context);
    require(retained->value->exactly_equal(value) == true, "later allocation damaged retained error");
}

// A compact shared graph can require exponential value traversal; the checked budget must stop it.
void comparison_budget(ProcessContext &context) {
    TermFactory factory(context);
    auto left = factory.nil().value();
    auto right = factory.nil().value();
    for (unsigned i = 0; i < 20; ++i) {
        left = factory.tuple(std::array{left, left}).value();
        right = factory.tuple(std::array{right, right}).value();
    }
    require(left.exactly_equal(right) == std::unexpected(TermError::resource_limit),
            "structural traversal ignored ceiling");
    GeneratedInvocation invocation(context.generated_calls());
    require(erlang_aot_exact_v1(&context, left.word(), right.word()) == 2, "comparison ceiling became inequality");
    require(context.generated_calls().failure()->status == Status::resource_limit, "comparison ceiling status lost");
}

// Foreign or out-of-heap words never reach a header read; expiration denies access while safely pinning storage.
void ownership(Runtime &runtime, ProcessContext &context, const Term &value) {
    auto &other = *runtime.create_context().value();
    require(Term::from_word(value.word(), other) == std::unexpected(TermError::wrong_owner), "foreign heap admitted");
    require(value.copy_to(other.heap()) == std::unexpected(TermError::wrong_owner), "foreign graph silently copied");
    require(Term::from_word(Word{1}, context) == std::unexpected(TermError::wrong_owner), "forged cons admitted");
    TermFactory factory(context);
    require(runtime.destroy_context(&context) == Status::ok, "context removal failed");
    require(value.tuple_element(0) == std::unexpected(TermError::expired_context), "expired graph accessed");
    require(factory.tuple({}) == std::unexpected(TermError::expired_context), "expired factory accessed");
}
} // namespace

int main() {
    try {
        auto runtime = Runtime::start().value();
        auto &context = *runtime->create_context({32, 4096 * sizeof(Word)}).value();
        const auto value = values(context);
        failure_payload(context, value);
        comparison_budget(context);
        ownership(*runtime, context, value);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

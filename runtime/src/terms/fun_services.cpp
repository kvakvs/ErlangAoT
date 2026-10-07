#include "funs.hpp"
#include "service_errors.hpp"
#include "terms.hpp"
#include <algorithm>
#include <array>
#include <erlang_aot/abi/calls.hpp>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/abi/funs.hpp>
#include <erlang_aot/runtime/process_context.hpp>
#include <new>
#include <vector>

// erlang_aot_make_fun_v1 and erlang_aot_apply_v1: building funs and preparing their calls (docs/funs.md).
namespace erlang_aot::runtime::detail {
namespace {
using abi::v1::ErrorReason;
using abi::v1::Status;

// Whether a service may run: an active invocation without a pending failure.
bool ready(ProcessContext &context) {
    const auto &calls = context.generated_calls();
    return calls.active() && !calls.failure();
}

// Build a fun of a registered descriptor from rooted captured values.
Status make(ProcessContext &context, const void *descriptor, std::span<const Word> captures, Word &output) {
    const auto *definition = context.code_server().fun_definition(descriptor);
    if (!definition) {
        return Status::wrong_owner;
    }
    const auto fun = TermFactory(context).fun_words(*definition, captures);
    if (!fun) {
        return term_status(fun.error());
    }
    output = fun->word();
    return Status::ok;
}

// Record the Erlang error a failed call raises: {badfun, Value}, {badarity, {Fun, Args}} or undef.
void raise(ProcessContext &context, ErrorReason reason, std::optional<Term> value = std::nullopt) {
    context.generated_calls().fail({.code = CallError::erlang_exception, .reason = reason, .value = std::move(value)});
}

// The {Fun, Args} payload of badarity, with the arguments in the registers.
TermResult<Term> bad_arity(ProcessContext &context, const Term &fun, std::span<const Word> arguments) {
    TermFactory factory(context);
    std::vector<Term> items;
    items.reserve(arguments.size());
    for (const auto word : arguments) {
        auto item = Term::from_word(word, context);
        if (!item) {
            return item;
        }
        items.push_back(std::move(*item));
    }
    const auto list = factory.list(items);
    return list.and_then([&](const Term &args) { return factory.tuple(std::array{fun, args}); });
}

// Check a called fun and append its captured values after the arguments; null when the call raised or failed.
const void *prepare(ProcessContext &context, const Term &fun, std::size_t arity, Word *arguments) {
    const auto view = fun_view(fun);
    if (!view) {
        raise(context, ErrorReason::badfun, fun);
        return nullptr;
    }
    const auto &definition = *view->definition;
    if (definition.arity != arity) {
        const auto payload = bad_arity(context, fun, {arguments, arity});
        if (payload) {
            raise(context, ErrorReason::badarity, *payload);
        } else {
            context.generated_calls().fail_service(term_status(payload.error()));
        }
        return nullptr;
    }
    if (!definition.frame) {
        raise(context, ErrorReason::undef);
        return nullptr;
    }
    std::ranges::copy(view->captures, arguments + arity);
    return definition.frame;
}

// Check the invocation and pointers, then build the fun; any failure is recorded in the channel.
std::uint8_t make_service(ProcessContext &context, const void *descriptor, const Word *captures, std::size_t count,
                          Word *output) noexcept {
    if (!ready(context)) {
        return static_cast<std::uint8_t>(Status::invalid_argument);
    }
    auto status = Status::invalid_argument;
    try {
        if (output && (count == 0 || captures)) {
            status = make(context, descriptor, {captures, count}, *output);
        }
    } catch (const std::bad_alloc &) {
        status = Status::out_of_memory;
    } catch (...) {
        status = Status::internal_error;
    }
    if (status != Status::ok) {
        context.generated_calls().fail_service(status);
    }
    return static_cast<std::uint8_t>(status);
}

// Admit the called value and check the register bounds before reading any argument.
const void *apply(ProcessContext &context, Word fun, Word *arguments, std::size_t arity) noexcept {
    if (!ready(context)) {
        return nullptr;
    }
    auto &calls = context.generated_calls();
    try {
        const auto value = Term::from_word(fun, context);
        if (!value) {
            calls.fail_service(term_status(value.error()));
            return nullptr;
        }
        const auto view = fun_view(*value);
        const auto captures = view ? view->captures.size() : 0;
        if (!arguments || arity + captures > abi::v1::register_count) {
            calls.fail_service(Status::invalid_argument);
            return nullptr;
        }
        return prepare(context, *value, arity, arguments);
    } catch (const std::bad_alloc &) {
        calls.fail_service(Status::out_of_memory);
    } catch (...) {
        calls.fail_service(Status::internal_error);
    }
    return nullptr;
}
} // namespace
} // namespace erlang_aot::runtime::detail

std::uint8_t erlang_aot_make_fun_v1(void *context, const void *descriptor,
                                    const erlang_aot::abi::v1::TermWord *captures, std::size_t count,
                                    erlang_aot::abi::v1::TermWord *output) noexcept {
    if (!context) {
        return static_cast<std::uint8_t>(erlang_aot::abi::v1::Status::invalid_argument);
    }
    return erlang_aot::runtime::detail::make_service(*static_cast<erlang_aot::runtime::ProcessContext *>(context),
                                                     descriptor, captures, count, output);
}

const void *erlang_aot_apply_v1(void *context, erlang_aot::abi::v1::TermWord fun, std::size_t arity,
                                erlang_aot::abi::v1::TermWord *arguments) noexcept {
    if (!context) {
        return nullptr;
    }
    return erlang_aot::runtime::detail::apply(*static_cast<erlang_aot::runtime::ProcessContext *>(context), fun,
                                              arguments, arity);
}

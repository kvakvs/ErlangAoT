#include <array>
#include <erlang_aot/abi/builtins.hpp>
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::abi::v1 {
namespace {
using namespace runtime;

// Keep generated callers independent of host std::expected and CallFailure layout.
Status call_status(CallError error) noexcept {
    switch (error) {
    case CallError::bad_arity:
    case CallError::argument_type_mismatch:
        return Status::invalid_argument;
    case CallError::wrong_owner:
        return Status::wrong_owner;
    case CallError::expired_context:
        return Status::stopped;
    case CallError::resource_limit:
        return Status::resource_limit;
    case CallError::native_exception:
        return Status::internal_error;
    case CallError::not_implemented:
        return Status::not_implemented;
    case CallError::diagnostic_failure:
        return Status::diagnostic_failure;
    }
    return Status::internal_error;
}

// A missing module/signature is an unavailable BIF, not an invented return value.
Status lookup_status(CodeError error, std::string_view module, std::string_view function) noexcept {
    if (error == CodeError::resource_limit) {
        return Status::resource_limit;
    }
    FeatureFailure failure;
    return failure.report(FeatureId::builtins, {.module = module, .operation = function});
}

// Adapt calling shape with bounded stack storage, without numeric/native conversions.
Status invoke(const ResolvedFunction &target, Context &context, const TermWord *arguments, std::size_t arity,
              TermWord &output) {
    std::array<Term, 255> terms;
    for (std::size_t index = 0; index < arity; ++index) {
        auto term = Term::from_word(arguments[index]);
        if (!term) {
            return Status::invalid_argument;
        }
        terms[index] = *term;
    }
    const auto result = target.call(context, std::span<const Term>(terms.data(), arity));
    if (!result) {
        return call_status(result.error().code);
    }
    output = result->word();
    return Status::ok;
}

// Resolve and invoke within a single pin while containing allocation and unexpected host failures.
Status dispatch(Context &context, std::string_view module, std::string_view function, const TermWord *arguments,
                std::size_t arity, TermWord &output) noexcept {
    try {
        const auto target = context.code_server().resolve({.module = module, .function = function, .arity = arity});
        if (!target) {
            return lookup_status(target.error(), module, function);
        }
        return invoke(*target, context, arguments, arity, output);
    } catch (const std::bad_alloc &) {
        return Status::out_of_memory;
    } catch (...) {
        return Status::internal_error;
    }
}
} // namespace

Status dispatch_builtin(Context *context, const char *module, std::size_t module_size, const char *function,
                        std::size_t function_size, const TermWord *arguments, std::size_t arity,
                        TermWord *result) noexcept {
    if (context == nullptr || module == nullptr || function == nullptr || result == nullptr) {
        return Status::invalid_argument;
    }
    if (module_size == 0 || function_size == 0 || arity > 255 || (arity != 0 && arguments == nullptr)) {
        return Status::invalid_argument;
    }
    return dispatch(*context, {module, module_size}, {function, function_size}, arguments, arity, *result);
}
} // namespace erlang_aot::abi::v1

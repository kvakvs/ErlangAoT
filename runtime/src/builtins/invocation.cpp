#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
namespace {
// Validate every argument before entering native code, preserving its exact failure index.
CallResult<void> validate_arguments(std::span<const Term> arguments) {
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto checked = Term::from_word(arguments[index].word());
        if (!checked) {
            return std::unexpected(CallFailure{CallError::argument_type_mismatch, index, checked.error()});
        }
    }
    return {};
}

// Report an unavailable implementation at the invocation owner, never at propagation sites.
CallResult<Term> check_result(CallResult<Term> result, std::string_view module, DiagnosticSink sink) {
    if (!result) {
        if (result.error().code == CallError::not_implemented && !result.error().reported) {
            FeatureFailure failure(sink);
            if (failure.report(abi::v1::FeatureId::builtins, {.module = module, .operation = "native invocation"}) !=
                abi::v1::Status::not_implemented) {
                return std::unexpected(CallFailure{CallError::diagnostic_failure});
            }
            result.error().reported = true;
        }
        return result;
    }
    const auto checked = Term::from_word(result->word());
    if (!checked) {
        return std::unexpected(CallFailure{CallError::argument_type_mismatch, {}, checked.error()});
    }
    return result;
}
} // namespace

ResolvedFunction::ResolvedFunction(std::shared_ptr<const LoadedModule> module, const Callable *target,
                                   std::size_t arity)
    : module_(std::move(module)), target_(target), arity_(arity) {}

std::size_t ResolvedFunction::arity() const noexcept { return arity_; }

CallResult<Term> ResolvedFunction::call(ProcessContext &context, std::span<const Term> arguments,
                                        DiagnosticSink sink) const noexcept {
    if (arguments.size() != arity_) {
        return std::unexpected(CallFailure{CallError::bad_arity});
    }
    const auto checked = validate_arguments(arguments);
    if (!checked) {
        return std::unexpected(checked.error());
    }
    try {
        return check_result((*target_)(context, arguments), module_->name(), sink);
    } catch (const std::bad_alloc &) {
        return std::unexpected(CallFailure{CallError::resource_limit});
    } catch (...) {
        return std::unexpected(CallFailure{CallError::native_exception});
    }
}
} // namespace erlang_aot::runtime

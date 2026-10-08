#include <clause/runtime/code_server.hpp>
#include <clause/runtime/process_context.hpp>

namespace clause::runtime {
namespace {
// Validate every argument before entering native code, preserving its exact failure index.
CallResult<void> validate_arguments(ProcessContext &context, std::span<const Term> arguments) {
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto checked = Term::from_word(arguments[index].word(), context);
        if (!checked) {
            return std::unexpected(CallFailure{CallError::argument_type_mismatch, index, checked.error()});
        }
    }
    return {};
}

// Validate native error payload ownership before propagating or reporting an unavailable body.
CallResult<Term> failed_result(ProcessContext &context, CallFailure failure, std::string_view module,
                               DiagnosticSink sink) {
    if (failure.value) {
        const auto checked = Term::from_word(failure.value->word(), context);
        if (!checked) {
            return std::unexpected(CallFailure{CallError::argument_type_mismatch, {}, checked.error()});
        }
    }
    if (failure.code != CallError::not_implemented || failure.reported) {
        return std::unexpected(failure);
    }
    FeatureFailure report(sink);
    if (report.report(abi::v1::FeatureId::builtins, {.module = module, .operation = "native invocation"}) !=
        abi::v1::Status::not_implemented) {
        return std::unexpected(CallFailure{CallError::diagnostic_failure});
    }
    failure.reported = true;
    return std::unexpected(failure);
}

// Check both successful values and error payloads at the host invocation boundary.
CallResult<Term> check_result(ProcessContext &context, CallResult<Term> result, std::string_view module,
                              DiagnosticSink sink) {
    if (!result) {
        return failed_result(context, result.error(), module, sink);
    }
    const auto checked = Term::from_word(result->word(), context);
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
    if (module_->atoms()) {
        const auto local = context.code_server().find_module(module_->name());
        if (!local || local->get() != module_.get()) {
            return std::unexpected(CallFailure{CallError::wrong_owner});
        }
    }
    const auto checked = validate_arguments(context, arguments);
    if (!checked) {
        return std::unexpected(checked.error());
    }
    try {
        return check_result(context, (*target_)(context, arguments), module_->name(), sink);
    } catch (const std::bad_alloc &) {
        return std::unexpected(CallFailure{CallError::resource_limit});
    } catch (...) {
        return std::unexpected(CallFailure{CallError::native_exception});
    }
}
} // namespace clause::runtime

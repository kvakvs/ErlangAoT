#include <erlang_aot/runtime/process_context.hpp>

namespace erlang_aot::runtime {
ProcessResult<void> ProcessContext::send(ProcessIdentity, const Term &, DiagnosticSink sink) noexcept {
    return deferred_service<ProcessError>(abi::v1::FeatureId::message_passing, "ProcessContext::send", sink);
}
} // namespace erlang_aot::runtime

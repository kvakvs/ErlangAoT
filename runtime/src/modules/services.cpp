#include <erlang_aot/runtime/code_server.hpp>

namespace erlang_aot::runtime {
CodeResult<void> CodeServer::unload(std::string_view name, DiagnosticSink sink) noexcept {
    if (!modules_.contains(name)) {
        return std::unexpected(CodeError::module_not_found);
    }
    return deferred_service<CodeError>(abi::v1::FeatureId::dynamic_modules, "CodeServer::unload", sink);
}
} // namespace erlang_aot::runtime

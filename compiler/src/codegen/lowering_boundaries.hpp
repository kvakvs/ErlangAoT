#pragma once
#include "features.hpp"

namespace erlang_aot::codegen {
// Keep concrete lowering extension points explicit until their semantic/runtime contracts exist.
enum class DeferredOperation : std::uint8_t { heap_value, dynamic_call, closure, exception, receive, send, sequence };
// Defensive lowering failures invalidate staged outputs even if a future caller bypasses capability checks.
bool reject_lowering_operation(CompilationResult &result, DeferredOperation operation,
                               const abi::v1::FeatureContext &context = {}, std::ostream &errors = std::cerr) noexcept;
} // namespace erlang_aot::codegen

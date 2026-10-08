#include "lowering_boundaries.hpp"

namespace clause::codegen {
namespace {
// Use compiler-owned syntax features rather than misreporting runtime service failures.
abi::v1::FeatureId feature(const DeferredOperation operation) noexcept {
    using enum DeferredOperation;
    using abi::v1::FeatureId;
    switch (operation) {
    case heap_value:
        return FeatureId::heap_expressions;
    case dynamic_call:
        return FeatureId::dynamic_calls;
    case closure:
        return FeatureId::closures;
    case exception:
        return FeatureId::exceptions;
    case receive:
        return FeatureId::receive;
    case send:
        return FeatureId::send_expressions;
    case sequence:
        return FeatureId::expression_sequences;
    }
    return FeatureId::invalid;
}
} // namespace

bool reject_lowering_operation(CompilationResult &result, const DeferredOperation operation,
                               const abi::v1::FeatureContext &context, std::ostream &errors) noexcept {
    return reject_feature(result, feature(operation), context, errors);
}
} // namespace clause::codegen

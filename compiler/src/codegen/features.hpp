#pragma once
#include "result.hpp"
#include <erlang_aot/abi/feature_diagnostic.hpp>
#include <iostream>

namespace erlang_aot::codegen {
// Fail an incomplete batch once, retaining the formatted diagnostic and emitting it regardless of verbosity.
// Default stderr is replaceable for embedding/tests; callers propagate false without printing it again.
bool reject_feature(CompilationResult &result, abi::v1::FeatureId feature, const abi::v1::FeatureContext &context = {},
                    std::ostream &errors = std::cerr) noexcept;
} // namespace erlang_aot::codegen

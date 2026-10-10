#pragma once
#include "declarations.hpp"
#include <clause/abi/features.hpp>

namespace clause::semantic {
// Map closed capability reasons to the shared compiler-owned feature catalog.
abi::v1::FeatureId capability_feature(std::string_view reason);
// Report the owning capability failure once, retaining source/include provenance.
// `operation` names what was rejected, such as an attribute; a warning reports a deferred feature that is skipped.
void reject_capability(const Module &module, const ast::NodeSource &source, std::string_view reason,
                       const Reporter &out, std::string_view operation = {}, Severity severity = Severity::error);
} // namespace clause::semantic

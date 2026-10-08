#include "features.hpp"
#include <clause/abi/feature_diagnostic.hpp>

namespace clause::semantic {
abi::v1::FeatureId capability_feature(const std::string_view reason) {
    for (const auto &feature : abi::v1::feature_catalog) {
        if (feature.owner == abi::v1::FeatureOwner::compiler && feature.name == reason) {
            return feature.id;
        }
    }
    throw std::logic_error("unregistered compiler capability: " + std::string(reason));
}

void reject_capability(const Module &module, const ast::NodeSource &source, const std::string_view reason,
                       const Reporter &out) {
    const auto name = utf8(module.name);
    report(module, &source, abi::v1::format_feature_failure(capability_feature(reason), {.module = name}), out);
}
} // namespace clause::semantic

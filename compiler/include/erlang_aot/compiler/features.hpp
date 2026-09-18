#pragma once
#include <memory>
#include <string>
#include <vector>

namespace erlang_aot {
struct FeatureContext {
    // Preserve enabled source-language features independently of keyword token spelling.
    std::vector<std::string> enabled;
};

// Null means unspecified for raw-token callers; preprocessing always supplies a snapshot.
using FeatureSnapshot = std::shared_ptr<const FeatureContext>;
} // namespace erlang_aot

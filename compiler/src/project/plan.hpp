#pragma once
#include "discovery.hpp"
#include "options.hpp"

namespace erlang_aot::project {
struct PlanOptions {
    // Preserve CLI target order, invocation path base, and per-invocation overrides.
    std::vector<std::string> selectors;
    std::filesystem::path working_directory;
    PreprocessorOptions preprocessing;
    std::optional<std::filesystem::path> output;
    // Frontend requests ignore manifest output paths and never plan executable writes.
    bool frontend = false;
    DiscoveryLimits discovery;
};

struct PlannedTarget {
    // Own all resolved target work independently of the manifest decoding tree.
    std::string name;
    std::vector<std::filesystem::path> sources;
    PreprocessorOptions preprocessing;
    // Keep future output destinations absent for check/print requests.
    std::optional<std::filesystem::path> output;
};

struct Invocation {
    // Preserve project identity, selected target order, and execution mode.
    std::filesystem::path file;
    std::vector<PlannedTarget> targets;
    bool frontend = false;
};

// Resolve and validate all selected target work before any frontend processing.
Invocation prepare(const Manifest &manifest, const PlanOptions &options);
} // namespace erlang_aot::project

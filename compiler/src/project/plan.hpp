#pragma once
#include "discovery.hpp"
#include "entry.hpp"
#include "options.hpp"

namespace clause::project {
struct PlanOptions {
    // Preserve CLI target order, invocation path base, and per-invocation overrides.
    std::vector<std::string> selectors;
    std::filesystem::path working_directory;
    PreprocessorOptions preprocessing;
    std::optional<std::filesystem::path> output;
    // CLI --entry replaces the manifest entry of the single selected target.
    std::optional<EntryName> entry;
    // Frontend (check/print/emit/inspect) requests ignore manifest output paths and never plan executable writes.
    bool frontend = false;
    DiscoveryLimits discovery;
};

struct PlannedTarget {
    // Own all resolved target work independently of the manifest decoding tree.
    std::string name;
    std::vector<std::filesystem::path> sources;
    PreprocessorOptions preprocessing;
    // Executable destination; absent for check/print requests and targets without output, entry, -o or --entry.
    std::optional<std::filesystem::path> output;
    // Explicit entry selection (CLI or manifest); absent means auto-detection when an executable is requested.
    std::optional<SelectedEntry> entry;
    // Expanded source_search_paths, also searched for a module the batch names but no source defines.
    std::vector<std::filesystem::path> search_paths_ = {};
};

struct Invocation {
    // Preserve project identity, selected target order, and execution mode.
    std::filesystem::path file;
    std::vector<PlannedTarget> targets;
    bool frontend = false;
};

// Resolve and validate all selected target work before any frontend processing.
Invocation prepare(const Manifest &manifest, const PlanOptions &options);
} // namespace clause::project

#pragma once
#include "../semantic/declarations.hpp"
#include "project/entry.hpp"
#include <clause/compiler/diagnostic.hpp>
#include <memory>
#include <span>

namespace clause::cli {
struct EntryRequest {
    // Explicit selection, validated whenever the batch is analyzed.
    std::optional<project::SelectedEntry> selected;
    // Executables need an entry, so an absent selection is detected from the batch's main/1 exports.
    bool required = false;
    // Project targets can also select the entry with the manifest key, so their hints mention it.
    bool project = false;
};

struct ResolvedEntry {
    // Batch index of the entry module and the function the startup code will call: arity 1 receives the argument
    // list, arity 0 runs without it.
    std::size_t module = 0;
    semantic::FunctionKey function;
    // Escript entries exit with status 127 on uncaught exceptions, like OTP escript.
    bool escript = false;
};

// Resolve the entry against indexed modules, reporting located errors; returns true on failure.
bool resolve_entry(std::span<const std::unique_ptr<semantic::Module>> modules, const EntryRequest &request,
                   const semantic::Reporter &report, const DiagnosticSink &sink, std::optional<ResolvedEntry> &entry);
} // namespace clause::cli

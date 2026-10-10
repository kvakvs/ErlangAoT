#include "entry.hpp"
#include <algorithm>
#include <clause/compiler/source.hpp>

namespace clause::cli {
namespace {
using Modules = std::span<const std::unique_ptr<semantic::Module>>;

// Explain how to choose an entry; only project targets have the manifest key.
std::string selection_hint(const bool project) {
    const std::string manifest =
        project ? " or with entry = \"MODULE[:FUNCTION]\" in this target's [[targets]] table of the project manifest"
                : "";
    return "; choose the entry with --entry MODULE[:FUNCTION]" + manifest +
           " (an exported FUNCTION/1, or FUNCTION/0 run without the program arguments; FUNCTION defaults to main)";
}

// Find the batch module whose declared name matches the requested atom text.
std::optional<std::size_t> find_module(const Modules modules, const std::u32string &name) {
    for (std::size_t index = 0; index < modules.size(); ++index) {
        if (modules[index]->declaration && modules[index]->name == name) {
            return index;
        }
    }
    return std::nullopt;
}

// Locate a same-named definition with another arity so wrong-arity errors point at it.
const semantic::Function *other_arity(const semantic::Module &module, const std::u32string &name) {
    for (const auto &function : module.functions) {
        if (function.key.name == name) {
            return &function;
        }
    }
    return nullptr;
}

// Point at a function definition, falling back to the module declaration.
const ast::NodeSource *source(const semantic::Module &module, const semantic::Function *function) {
    if (function) {
        return &module.syntax->form(function->form).source;
    }
    return module.declaration ? &module.syntax->form(*module.declaration).source : nullptr;
}

// The selected entry definition: FUNCTION/1, which receives the argument list, else FUNCTION/0, which runs without it.
const semantic::Function *entry_function(const semantic::Module &module, const std::u32string &name) {
    for (const std::size_t arity : {1, 0}) {
        if (const auto found = module.lookup.find({name, arity}); found != module.lookup.end()) {
            return &module.functions[found->second];
        }
    }
    return nullptr;
}

// Require FUNCTION/1 or FUNCTION/0 to exist and be exported; returns its arity.
std::optional<std::size_t> check_function(const semantic::Module &module, const project::EntryName &name,
                                          const semantic::Reporter &report) {
    const auto text = "entry function " + project::entry_text(name);
    const auto *function = entry_function(module, name.function);
    if (!function) {
        const auto *other = other_arity(module, name.function);
        const auto detail = other ? "; found " + utf8(name.function) + "/" + std::to_string(other->key.arity) +
                                        ", but the entry takes the argument list (FUNCTION/1) or nothing (FUNCTION/0)"
                                  : std::string{};
        semantic::report(module, source(module, other), text + "/1 is not defined" + detail, report);
        return std::nullopt;
    }
    if (!function->exported) {
        semantic::report(module, source(module, function),
                         text + "/" + std::to_string(function->key.arity) + " is not exported", report);
        return std::nullopt;
    }
    return function->key.arity;
}

// Check an explicit selection; unknown modules are reported against the selection's origin.
std::optional<ResolvedEntry> check_selected(const Modules modules, const project::SelectedEntry &selected,
                                            const semantic::Reporter &report, const DiagnosticSink &sink) {
    const auto index = find_module(modules, selected.name.module);
    if (!index) {
        sink("error: " + selected.origin + ": entry module " + utf8(selected.name.module) +
             " is not among the compiled modules");
        return std::nullopt;
    }
    const auto arity = check_function(*modules[*index], selected.name, report);
    if (!arity) {
        return std::nullopt;
    }
    return ResolvedEntry{*index, {selected.name.function, *arity}, modules[*index]->escript};
}

// Report whether a module can serve as the default entry: escripts first, then main/1 exporters.
bool candidate(const semantic::Module &module, const bool escripts) {
    const auto found = module.lookup.find({U"main", 1});
    return module.escript == escripts && found != module.lookup.end() && module.functions[found->second].exported;
}

// Pick the only escript, else the only module exporting main/1, when no entry was selected.
std::optional<ResolvedEntry> detect(Modules modules, const bool project, const DiagnosticSink &sink) {
    const bool escripts = std::ranges::any_of(modules, [](const auto &module) { return module->escript; });
    std::vector<std::size_t> candidates;
    std::string names;
    for (std::size_t index = 0; index < modules.size(); ++index) {
        if (candidate(*modules[index], escripts)) {
            names += (candidates.empty() ? "" : ", ") + utf8(modules[index]->name);
            candidates.push_back(index);
        }
    }
    if (candidates.size() == 1) {
        return ResolvedEntry{candidates.front(), {U"main", 1}, escripts};
    }
    sink((candidates.empty() ? "error: no entry point: no module exports main/1"
                             : "error: ambiguous entry point: main/1 is exported by " + names) +
         selection_hint(project));
    return std::nullopt;
}
} // namespace

bool resolve_entry(const Modules modules, const EntryRequest &request, const semantic::Reporter &report,
                   const DiagnosticSink &sink, std::optional<ResolvedEntry> &entry) {
    if (request.selected) {
        entry = check_selected(modules, *request.selected, report, sink);
    } else if (request.required) {
        entry = detect(modules, request.project, sink);
    } else {
        return false;
    }
    return !entry.has_value();
}
} // namespace clause::cli

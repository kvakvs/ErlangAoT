#include "declarations.hpp"
#include "behaviours.hpp"
#include "escript.hpp"
#include "records.hpp"
#include "symbols.hpp"
#include <charconv>
#include <clause/compiler/source.hpp>

namespace clause::semantic {
void report(const Module &module, const ast::NodeSource *source, std::string message, const Reporter &reporter,
            const Severity severity) {
    if (source) {
        report(module.syntax->anchor(*source), std::move(message), reporter, severity);
        return;
    }
    reporter({.code = DiagnosticCode::invalid_context,
              .message = std::move(message),
              .primary = {},
              .related = {},
              .severity = severity,
              .location = LogicalLocation{module.file, 1, 1}});
}

void report(const ast::TokenOrigin &origin, std::string message, const Reporter &reporter, const Severity severity) {
    reporter({.code = DiagnosticCode::invalid_context,
              .message = std::move(message),
              .primary = origin.spelling,
              .related = origin.related,
              .severity = severity,
              .location = origin.location});
}

std::optional<std::size_t> arity(const Integer &value) {
    std::size_t result = 0;
    const auto &text = value.decimal;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || result > 255) {
        return {};
    }
    return result;
}

std::optional<std::size_t> source_function(const Module &module, const FunctionKey &key) {
    const auto found = module.lookup.find(key);
    if (found == module.lookup.end() || (module.behaviour_info_ && key == FunctionKey{U"behaviour_info", 1})) {
        return std::nullopt;
    }
    return found->second;
}

namespace {
// Keep a module's first declaration as its identity while reporting subsequent declarations.
void module_name(Module &module, const ast::FormId &id, const ast::ModuleAttribute &attribute, const Reporter &out) {
    const auto &source = module.syntax->form(id).source;
    if (module.declaration) {
        report(module, &source, "duplicate module declaration", out);
        return;
    }
    module.declaration = id;
    module.name = attribute.name.name;
    if (module.name.empty() || module.name.size() > 255) {
        report(module, &source, "invalid module name", out);
    }
}

// Retain unique function definitions in source order, rejecting noncontiguous duplicates.
void function(Module &module, const ast::FormId &id, const ast::Function &syntax, const Reporter &out) {
    FunctionKey key{syntax.name.name, syntax.clauses.front().arguments.size()};
    const auto &source = module.syntax->form(id).source;
    if (key.arity > 255) {
        report(module, &source, "function arity exceeds 255", out);
        return;
    }
    if (key == FunctionKey{U"record_info", 2}) {
        // OTP expands record_info/2 at compile time, so the module already defines it; index it anyway.
        report(module, &source, "function record_info/2 already defined", out);
    }
    if (!module.lookup.emplace(key, module.functions.size()).second) {
        report(module, &source, "duplicate function " + utf8(key.name) + "/" + std::to_string(key.arity), out);
        return;
    }
    module.functions.push_back({key, id, false, encode_symbol({utf8(module.name), utf8(key.name), key.arity})});
}

// Resolve each export after all functions are indexed, allowing forward declarations.
void export_function(Module &module, const ast::NameArity &value, const ast::NodeSource &source, const Reporter &out) {
    const auto count = arity(value.arity);
    if (!count) {
        report(module, &source, "export arity must be in 0..255", out);
        return;
    }
    const auto found = source_function(module, {value.name.name, *count});
    if (!found) {
        report(module, &source, "export of undefined function " + utf8(value.name.name) + "/" + std::to_string(*count),
               out);
        return;
    }
    auto &entry = module.functions[*found];
    if (entry.exported) {
        report(module, &source, "duplicate export " + utf8(value.name.name) + "/" + std::to_string(*count), out);
    }
    entry.exported = true;
}
} // namespace

namespace {
// Establish module identity before constructing function symbols.
void index_name(Module &module, const Reporter &out) {
    const auto &syntax = *module.syntax;
    for (const auto &id : syntax.forms()) {
        if (const auto *attribute = std::get_if<ast::ModuleAttribute>(&syntax.form(id).value)) {
            module_name(module, id, *attribute, out);
        }
    }
    if (!module.declaration) {
        const auto *source = syntax.forms().empty() ? nullptr : &syntax.form(syntax.forms().front()).source;
        report(module, source, "missing module declaration", out);
    }
}

// Index the complete function set independently of export declaration order.
void index_functions(Module &module, const Reporter &out) {
    const auto &syntax = *module.syntax;
    for (const auto &id : syntax.forms()) {
        if (const auto *value = std::get_if<ast::Function>(&syntax.form(id).value)) {
            function(module, id, *value, out);
        }
    }
}

// Report each malformed export without hiding later declarations.
void index_exports(Module &module, const Reporter &out) {
    const auto &syntax = *module.syntax;
    for (const auto &id : syntax.forms()) {
        const auto &form = syntax.form(id);
        if (const auto *exports = std::get_if<ast::ExportAttribute>(&form.value)) {
            for (const auto &value : exports->functions) {
                export_function(module, value, form.source, out);
            }
        }
    }
}
} // namespace

std::unique_ptr<Module> index(const ast::Module &syntax, std::string file, const Reporter &out,
                              const IndexOptions options) {
    auto module = std::make_unique<Module>();
    module->syntax = &syntax;
    module->file = std::move(file);
    module->behaviour_info_ = options.behaviour_info_;
    index_name(*module, out);
    index_functions(*module, out);
    index_exports(*module, out);
    index_records(*module, out);
    index_callbacks(*module, out);
    if (options.escript_) {
        index_escript(*module, out);
    }
    return module;
}
} // namespace clause::semantic

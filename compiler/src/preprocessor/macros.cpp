#include "macros.hpp"
#include "token_utils.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {
// These variable spellings bypass ordinary table dispatch in epp, even after undef.
bool special_context(const Token &name) {
    static const std::set<std::u32string_view> special{U"LINE", U"FUNCTION_NAME", U"FUNCTION_ARITY"};
    return name.kind == TokenKind::variable && special.contains(name.text());
}

// Record every reference without expanding stored bodies or discarding unused arguments.
std::vector<MacroKey> dependencies(std::span<const Token> body) {
    std::vector<MacroKey> result;
    for (std::size_t i = 0; i + 1 < body.size(); ++i) {
        if (!syntax(body[i], U"?")) {
            continue;
        }
        const auto &name = body[++i];
        if (name.kind != TokenKind::atom && name.kind != TokenKind::variable) {
            continue;
        }
        std::optional<std::size_t> arity;
        if (i + 1 < body.size() && syntax(body[i + 1], U"(")) {
            arity = collect_arguments(body.subspan(i + 1), name).values.size();
        }
        result.push_back({std::u32string(name.text()), arity});
    }
    return result;
}

// Walk a dependency DAG once per invocation, retaining an active path for cycle diagnostics.
void visit(const MacroTable &table, const MacroKey &key, std::set<MacroKey> &active, std::set<MacroKey> &done,
           const Token &call, std::size_t limit) {
    if (active.contains(key)) {
        pp_fail(DiagnosticCode::macro_cycle, "circular macro " + utf8(key.name), call);
    }
    if (done.contains(key)) {
        return;
    }
    if (active.size() >= limit) {
        pp_fail(DiagnosticCode::resource_limit, "macro dependency depth exhausted", call);
    }
    auto found = table.definitions.find(key);
    if (found == table.definitions.end()) {
        found = table.definitions.find({key.name, std::nullopt});
    }
    if (found == table.definitions.end()) {
        return;
    }
    active.insert(key);
    for (const auto &dependency : dependencies(found->second.body)) {
        visit(table, dependency, active, done, call, limit);
    }
    active.erase(key);
    done.insert(key);
}
} // namespace

void MacroTable::check_cycles(const Definition &definition, const Token &call, std::size_t maximum_depth) const {
    std::set<MacroKey> active;
    std::set<MacroKey> done;
    const auto arity = definition.parameters ? std::optional(definition.parameters->size()) : std::nullopt;
    visit(*this, {std::u32string(definition.name.text()), arity}, active, done, call, maximum_depth);
}

void MacroTable::define(Definition definition) {
    const std::u32string name(definition.name.text());
    if (reserved.contains(name)) {
        pp_fail(DiagnosticCode::macro_redefinition, "redefining predefined macro " + utf8(name), definition.name);
    }
    MacroKey key{name, std::nullopt};
    if (definition.parameters) {
        key.arity = definition.parameters->size();
        std::set<std::u32string_view> names;
        for (const auto &parameter : *definition.parameters) {
            if (!names.insert(parameter.text()).second) {
                pp_fail(DiagnosticCode::macro_arguments, "duplicate macro parameter", parameter);
            }
        }
    }
    if (definitions.contains(key)) {
        pp_fail(DiagnosticCode::macro_redefinition, "redefining macro " + utf8(name), definition.name);
    }
    dependencies(definition.body);
    definitions.emplace(std::move(key), std::move(definition));
}

void MacroTable::undefine(std::u32string_view name) {
    std::erase_if(definitions, [name](const auto &item) { return item.first.name == name; });
    reserved.erase(std::u32string(name));
    undefined.erase(std::u32string(name));
}

bool MacroTable::contains(std::u32string_view name, bool include_undefined) const {
    const std::u32string key(name);
    if (undefined.contains(key)) {
        return include_undefined;
    }
    if (reserved.contains(key)) {
        return true;
    }
    return std::ranges::any_of(definitions, [name](const auto &item) { return item.first.name == name; });
}

const Definition *MacroTable::lookup(const Token &name, std::optional<std::size_t> arity) const {
    const std::u32string key(name.text());
    const auto count = std::ranges::count_if(definitions, [&key](const auto &item) { return item.first.name == key; });
    if (count == 1) {
        if (auto object = definitions.find({key, std::nullopt}); object != definitions.end()) {
            return &object->second;
        }
    }
    if (const auto found = definitions.find({key, arity}); found != definitions.end()) {
        return &found->second;
    }
    const auto code = count == 0 ? DiagnosticCode::undefined_macro : DiagnosticCode::macro_arguments;
    pp_fail(code, (count == 0 ? "undefined macro " : "macro arity mismatch: ") + utf8(key), name);
}

MacroExpander::MacroExpander(const MacroTable &table, const PreprocessorLimits &limits, Builtin builtin)
    : table_(table), limits_(limits), builtin_(std::move(builtin)) {}

void MacroExpander::budget(std::size_t count, const Token &call) {
    if (count > limits_.tokens - std::min(produced_, limits_.tokens)) {
        pp_fail(DiagnosticCode::resource_limit, "macro token budget exhausted", call);
    }
    produced_ += count;
}

namespace {
// Consume the double-question marker only when followed by a formal-shaped variable.
bool stringify_parameter(std::span<const Token> body, std::size_t &position) {
    if (position + 2 >= body.size()) {
        return false;
    }
    if (!syntax(body[position], U"?") || !syntax(body[position + 1], U"?")) {
        return false;
    }
    if (body[position + 2].kind != TokenKind::variable) {
        return false;
    }
    position += 2;
    return true;
}

// Retain dynamic ancestry too: substitution can manufacture references absent from the static graph.
void expansion_path(const Definition &definition, const Token &call, std::size_t limit) {
    const auto &site = definition.name.spelling;
    const auto recursive = [&](const Span &origin) {
        return origin.source == site.source && origin.begin == site.begin && origin.end == site.end;
    };
    if (std::ranges::any_of(call.origins, recursive)) {
        pp_fail(DiagnosticCode::macro_cycle, "circular macro expansion", call);
    }
    if (call.origins.size() / 2 >= limit) {
        pp_fail(DiagnosticCode::resource_limit, "macro expansion depth exhausted", call);
    }
}

// Apply the invocation annotation while recording original definition provenance.
Token replacement(Token token, const Token &call, const Definition &definition) {
    token.location = call.location;
    token.origins = call.origins;
    token.origins.push_back(definition.name.spelling);
    token.origins.push_back(call.spelling);
    return token;
}
} // namespace

std::vector<Token> MacroExpander::substitute(const Definition &definition, const Arguments &arguments,
                                             const Token &call) {
    table_.check_cycles(definition, call, limits_.expansion_depth);
    expansion_path(definition, call, limits_.expansion_depth);
    std::map<std::u32string_view, const std::vector<Token> *> bindings;
    if (definition.parameters) {
        for (std::size_t i = 0; i < arguments.values.size(); ++i) {
            bindings.emplace((*definition.parameters)[i].text(), &arguments.values[i]);
        }
    }
    std::vector<Token> result;
    auto location = call;
    for (std::size_t i = 0; i < definition.body.size(); ++i) {
        const bool stringified = stringify_parameter(definition.body, i);
        const auto &token = definition.body[i];
        const auto found = bindings.find(token.text());
        if (token.kind != TokenKind::variable || found == bindings.end()) {
            result.push_back(replacement(token, location, definition));
            continue;
        }
        if (stringified) {
            result.push_back(generated(location, TokenKind::string, stringify(*found->second)));
            continue;
        }
        result.insert(result.end(), found->second->begin(), found->second->end());
        // collect_arguments rejects empty actual arguments.
        location.location = found->second->back().location;
    }
    return result;
}

std::vector<Token> MacroExpander::reference(std::deque<Token> &pending) {
    const Token question = pending.front();
    pending.pop_front();
    if (pending.empty()) {
        pp_fail(DiagnosticCode::macro_arguments, "missing macro name", question);
    }
    const Token name = pending.front();
    pending.pop_front();
    if (name.kind != TokenKind::atom && name.kind != TokenKind::variable) {
        pp_fail(DiagnosticCode::macro_arguments, "expected macro name", name);
    }
    const auto builtin = builtin_(name);
    if (special_context(name) && builtin) {
        return *builtin;
    }
    Arguments args;
    if (!pending.empty() && syntax(pending.front(), U"(")) {
        args = collect_arguments(std::vector<Token>(pending.begin(), pending.end()), name);
    }
    if (builtin) {
        return *builtin;
    }
    return invoke(name, args, pending);
}

std::vector<Token> MacroExpander::invoke(const Token &name, const Arguments &args, std::deque<Token> &pending) {
    const auto *definition = table_.lookup(name, args.consumed ? std::optional(args.values.size()) : std::nullopt);
    if (definition->parameters) {
        for (std::size_t i = 0; i < args.consumed; ++i) {
            pending.pop_front();
        }
    }
    auto replacement = substitute(*definition, args, name);
    return definition->parameters ? replacement : rescan(replacement);
}

std::vector<Token> MacroExpander::expand(std::span<const Token> input) {
    if (input.empty()) {
        return {};
    }
    produced_ = 0;
    budget(input.size(), input.front());
    return rescan(input);
}

std::vector<Token> MacroExpander::rescan(std::span<const Token> input) {
    std::deque<Token> pending(input.begin(), input.end());
    std::vector<Token> output;
    while (!pending.empty()) {
        if (!syntax(pending.front(), U"?")) {
            output.push_back(std::move(pending.front()));
            pending.pop_front();
            continue;
        }
        const auto call = pending.front();
        auto expanded = reference(pending);
        budget(expanded.size() + 1, call);
        for (auto it = expanded.rbegin(); it != expanded.rend(); ++it) {
            pending.push_front(std::move(*it));
        }
    }
    return output;
}
} // namespace erlang_aot

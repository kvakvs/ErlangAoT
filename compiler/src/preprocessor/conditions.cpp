#include "engine.hpp"

namespace erlang_aot {
namespace {
// Structural branches share directive-envelope validation and move diagnostic ownership.
Directive checked_directive(std::span<const Token> tokens) {
    auto parsed = parse_directive(tokens);
    if (auto *error = std::get_if<Diagnostic>(&parsed)) {
        throw DiagnosticError(std::move(*error));
    }
    return std::get<Directive>(std::move(parsed));
}
} // namespace

bool PreprocessorSession::State::test_branch(const Directive &directive) {
    if (const auto *name = std::get_if<MacroName>(&directive.operand)) {
        const auto present = macros.contains(name->name.text());
        return directive.kind == DirectiveKind::ifdef ? present : !present;
    }
    const auto tokens = expand(std::get<TokenOperand>(directive.operand).tokens);
    if (tokens.empty()) {
        pp_fail(DiagnosticCode::invalid_condition, "expected condition expression",
                std::get<TokenOperand>(directive.operand).tokens.front());
    }
    return condition(tokens, options.limits.expression_depth,
                     [this](std::u32string_view name) { return macros.contains(name, true); });
}

void PreprocessorSession::State::begin_branch(DirectiveKind kind, std::span<const Token> tokens) {
    const bool parent = active();
    files.back().branches.push_back({tokens[1], parent, false, false, false});
    if (!parent) {
        return;
    }
    auto parsed = parse_directive(tokens);
    if (auto *error = std::get_if<Diagnostic>(&parsed)) {
        throw DiagnosticError(std::move(*error));
    }
    const bool selected = test_branch(std::get<Directive>(parsed));
    auto &branch = files.back().branches.back();
    branch.selected = selected;
    branch.active = selected;
    (void)kind;
}

void PreprocessorSession::State::change_branch(DirectiveKind kind, std::span<const Token> tokens) {
    auto &branches = files.back().branches;
    if (branches.empty()) {
        pp_fail(DiagnosticCode::conditional_structure, "unmatched conditional directive", tokens[1]);
    }
    auto &branch = branches.back();
    if (kind == DirectiveKind::endif) {
        if (branch.active) {
            checked_directive(tokens);
        }
        branches.pop_back();
        return;
    }
    select_branch(kind, tokens);
}

void PreprocessorSession::State::select_branch(DirectiveKind kind, std::span<const Token> tokens) {
    auto &branch = files.back().branches.back();
    if (branch.seen_else) {
        branch.active = false;
        pp_fail(DiagnosticCode::conditional_structure, "branch after else", tokens[1]);
    }
    const bool was_active = branch.active;
    branch.active = false;
    if (kind == DirectiveKind::else_branch) {
        if (was_active) {
            checked_directive(tokens);
        }
        branch.seen_else = true;
        branch.active = branch.parent && !branch.selected;
        branch.selected = true;
        return;
    }
    if (!branch.parent || branch.selected) {
        return;
    }
    auto parsed = parse_directive(tokens);
    if (auto *error = std::get_if<Diagnostic>(&parsed)) {
        throw DiagnosticError(std::move(*error));
    }
    branch.active = test_branch(std::get<Directive>(parsed));
    branch.selected = branch.active;
}

bool PreprocessorSession::State::conditional(DirectiveKind kind, std::span<const Token> tokens) {
    if (kind == DirectiveKind::ifdef || kind == DirectiveKind::ifndef || kind == DirectiveKind::if_condition) {
        begin_branch(kind, tokens);
        return true;
    }
    if (kind == DirectiveKind::else_branch || kind == DirectiveKind::elif || kind == DirectiveKind::endif) {
        change_branch(kind, tokens);
        return true;
    }
    return false;
}
} // namespace erlang_aot

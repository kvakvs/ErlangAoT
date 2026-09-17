#include "cursor.hpp"
#include <algorithm>
#include <array>
#include <erlang_aot/compiler/directive.hpp>

namespace erlang_aot {
namespace {
// Keep supported directive names separate from arbitrary Erlang attributes.
constexpr std::array names{U"define", U"undef", U"include", U"include_lib", U"ifdef", U"ifndef", U"if",
                           U"elif",   U"else",  U"endif",   U"feature",     U"error", U"warning"};

// Preserve the distinction between object definitions and a zero-arity
// definition.
Definition definition(DirectiveCursor &cursor) {
    Definition result{cursor.name(), std::nullopt, {}};
    if (cursor.take(U"(")) {
        result.parameters.emplace();
        if (!cursor.take(U")")) {
            do {
                result.parameters->push_back(cursor.category(TokenKind::variable, "formal parameter"));
            } while (cursor.take(U","));
            cursor.expect(U")");
        }
    }
    cursor.expect(U",");
    result.body = cursor.remainder();
    return result;
}

// Validate the fixed feature envelope without applying release-specific policy.
FeatureSetting feature(DirectiveCursor &cursor) {
    auto name = cursor.category(TokenKind::atom, "feature atom");
    cursor.expect(U",");
    auto action = cursor.category(TokenKind::atom, "enable or disable");
    if (action.text() != U"enable" && action.text() != U"disable") {
        throw Diagnostic{DiagnosticCode::malformed_directive, "expected enable or disable", action.spelling, {}};
    }
    cursor.finish();
    return {std::move(name), action.text() == U"enable"};
}

// Read the nonempty operands whose expanded term/expression grammar comes
// later.
TokenOperand deferred_operand(DirectiveCursor &cursor) {
    auto tokens = cursor.remainder();
    if (tokens.empty()) {
        cursor.fail("expected directive operand");
    }
    return {std::move(tokens)};
}

// Include operands can concatenate strings or start with a macro awaiting
// expansion.
TokenOperand include_operand(DirectiveCursor &cursor) {
    auto result = deferred_operand(cursor);
    const auto &first = result.tokens.front();
    if (first.kind == TokenKind::symbol && first.text() == U"?") {
        DirectiveCursor macro(std::span(result.tokens).subspan(1), first.spelling);
        macro.name();
        return result;
    }
    for (const auto &token : result.tokens) {
        if (token.kind != TokenKind::string) {
            throw Diagnostic{
                DiagnosticCode::malformed_directive, "expected include string or macro", token.spelling, {}};
        }
    }
    return result;
}

// Dispatch by operand family to keep syntax checks independent of session
// state.
DirectiveOperand operand(DirectiveKind kind, DirectiveCursor &cursor) {
    switch (kind) {
    case DirectiveKind::define:
        return definition(cursor);
    case DirectiveKind::undef:
    case DirectiveKind::ifdef:
    case DirectiveKind::ifndef: {
        MacroName result{cursor.name()};
        cursor.finish();
        return result;
    }
    case DirectiveKind::feature:
        return feature(cursor);
    case DirectiveKind::include:
    case DirectiveKind::include_lib:
        return include_operand(cursor);
    default:
        return deferred_operand(cursor);
    }
}

// Strip just the envelope: macro bodies may contain unmatched Erlang
// delimiters.
Directive parse(std::span<const Token> tokens, DirectiveKind kind) {
    const Span span{tokens.front().spelling.source, tokens.front().spelling.begin, tokens.back().spelling.end};
    auto input = tokens.subspan(2, tokens.size() - 3);
    DirectiveCursor cursor(input, tokens.back().spelling);
    if (kind == DirectiveKind::else_branch || kind == DirectiveKind::endif) {
        cursor.finish();
        return {kind, std::monostate{}, span};
    }
    cursor.expect(U"(");
    if (input.size() < 2 || input.back().kind != TokenKind::symbol || input.back().text() != U")") {
        throw Diagnostic{
            DiagnosticCode::malformed_directive, "expected closing ')' before '.'", tokens.back().spelling, {}};
    }
    DirectiveCursor arguments(input.subspan(1, input.size() - 2), input.back().spelling);
    return {kind, operand(kind, arguments), span};
}
} // namespace

std::optional<DirectiveKind> directive_kind(std::span<const Token> tokens) {
    if (tokens.size() < 2) {
        return std::nullopt;
    }
    if (tokens[0].kind != TokenKind::symbol || tokens[0].text() != U"-") {
        return std::nullopt;
    }
    if (tokens[1].kind != TokenKind::atom && tokens[1].kind != TokenKind::keyword) {
        return std::nullopt;
    }
    const auto found = std::ranges::find(names, tokens[1].text());
    if (found == names.end()) {
        return std::nullopt;
    }
    return static_cast<DirectiveKind>(found - names.begin());
}

std::variant<Directive, Diagnostic> parse_directive(std::span<const Token> tokens) {
    if (tokens.empty()) {
        return Diagnostic{DiagnosticCode::malformed_directive, "expected preprocessing directive", {}, {}};
    }
    if (tokens.back().kind != TokenKind::dot) {
        const auto &last = tokens.back().spelling;
        return Diagnostic{
            DiagnosticCode::missing_terminator, "expected form-ending '.'", {last.source, last.end, last.end}, {}};
    }
    const auto kind = directive_kind(tokens);
    if (!kind) {
        return Diagnostic{
            DiagnosticCode::malformed_directive, "expected preprocessing directive", tokens.front().spelling, {}};
    }
    try {
        return parse(tokens, *kind);
    } catch (const Diagnostic &error) {
        return error;
    }
}
} // namespace erlang_aot

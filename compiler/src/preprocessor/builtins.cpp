#include "engine.hpp"
#include <algorithm>
#include <charconv>

namespace erlang_aot {
namespace {
// LINE and function placeholders are special even after undefining their table entries.
std::optional<std::vector<Token>> special_builtin(const Token &name) {
    const auto text = name.text();
    if (name.kind == TokenKind::variable && text == U"LINE") {
        return std::vector{generated(name, TokenKind::integer, Integer{std::to_string(name.location.line)})};
    }
    if (name.kind == TokenKind::variable && (text == U"FUNCTION_NAME" || text == U"FUNCTION_ARITY")) {
        return std::vector{generated(name, TokenKind::variable, U"$" + std::u32string(text))};
    }
    return std::nullopt;
}
} // namespace

std::optional<std::vector<Token>> PreprocessorSession::State::builtin(const Token &name) {
    const auto text = name.text();
    if (const auto value = special_builtin(name)) {
        return value;
    }
    if (!macros.reserved.contains(std::u32string(text))) {
        return std::nullopt;
    }
    if (macros.undefined.contains(std::u32string(text))) {
        pp_fail(DiagnosticCode::undefined_macro, "undefined contextual macro " + utf8(text), name);
    }
    if (text == U"FILE") {
        Source converted(0, "<filename>", files.back().logical_name);
        return std::vector{generated(name, TokenKind::string, converted.text)};
    }
    static const std::map<std::u32string_view, TokenValue> constants{{U"MACHINE", std::u32string(U"BEAM")},
                                                                     {U"BEAM", std::u32string(U"true")},
                                                                     {U"OTP_RELEASE", Integer{"29"}},
                                                                     {U"LINE", Integer{"1"}}};
    if (const auto found = constants.find(text); found != constants.end()) {
        const auto kind = std::holds_alternative<Integer>(found->second) ? TokenKind::integer : TokenKind::atom;
        return std::vector{generated(name, kind, found->second)};
    }
    return module_builtin(name);
}

std::optional<std::vector<Token>> PreprocessorSession::State::module_builtin(const Token &name) {
    const auto text = name.text();
    const auto &context = text.starts_with(U"BASE_") ? base_module : module;
    if (!context) {
        pp_fail(DiagnosticCode::undefined_macro, "undefined contextual macro " + utf8(text), name);
    }
    return std::vector{generated(name, text.ends_with(U"STRING") ? TokenKind::string : TokenKind::atom,
                                 std::u32string(context->text()))};
}

void PreprocessorSession::State::function_context(std::vector<Token> &tokens) {
    const auto marker = [](const Token &token) {
        return token.kind == TokenKind::variable && token.text().starts_with(U"$FUNCTION_");
    };
    const auto found = std::ranges::find_if(tokens, marker);
    if (found == tokens.end()) {
        return;
    }
    if (tokens.size() < 3 || tokens[0].kind != TokenKind::atom || !syntax(tokens[1], U"(")) {
        pp_fail(DiagnosticCode::invalid_context, "function macro outside a function body", *found);
    }
    const auto args = collect_arguments(std::span(tokens).subspan(1), tokens[0]);
    if (static_cast<std::size_t>(found - tokens.begin()) < args.consumed + 1) {
        pp_fail(DiagnosticCode::invalid_context, "function macro in function header", *found);
    }
    const auto name = std::u32string(tokens[0].text());
    for (auto &token : tokens) {
        if (!marker(token)) {
            continue;
        }
        token = token.text() == U"$FUNCTION_NAME"
                    ? generated(token, TokenKind::atom, name)
                    : generated(token, TokenKind::integer, Integer{std::to_string(args.values.size())});
    }
}

std::vector<Token> PreprocessorSession::State::expand(std::span<const Token> tokens, bool function) {
    auto result =
        MacroExpander(macros, options.limits, [this](const Token &name) { return builtin(name); }).expand(tokens);
    if (function) {
        function_context(result);
    } else {
        for (const auto &token : result) {
            if (token.kind == TokenKind::variable && token.text().starts_with(U"$FUNCTION_")) {
                pp_fail(DiagnosticCode::invalid_context, "function macro outside a function body", token);
            }
        }
    }
    return result;
}

void PreprocessorSession::State::emit_file(const Token &site, const std::string &name, std::size_t line) {
    auto tokens = fragment("-file(\"\",1).");
    Source converted(0, "<filename>", name);
    tokens[3] = generated(site, TokenKind::string, converted.text);
    tokens[5] = generated(site, TokenKind::integer, Integer{std::to_string(line)});
    for (auto &token : tokens) {
        token.location = site.location;
        token.origins.push_back(site.spelling);
    }
    pending.emplace_back(OrdinaryForm{std::move(tokens), feature_snapshot});
}

void PreprocessorSession::State::file_mapping(const std::vector<Token> &tokens) {
    if (tokens.size() != 8 || tokens[3].kind != TokenKind::string || tokens[5].kind != TokenKind::integer) {
        pp_fail(DiagnosticCode::malformed_directive, "invalid file attribute", tokens[1]);
    }
    if (!syntax(tokens[2], U"(") || !syntax(tokens[4], U",") || !syntax(tokens[6], U")")) {
        pp_fail(DiagnosticCode::malformed_directive, "invalid file attribute", tokens[1]);
    }
    const auto &digits = std::get<Integer>(tokens[5].value).decimal;
    std::size_t line = 0;
    const auto conversion = std::from_chars(digits.data(), digits.data() + digits.size(), line);
    if (conversion.ec != std::errc{} || line > 1000000000) {
        pp_fail(DiagnosticCode::resource_limit, "logical line limit exceeded", tokens[5]);
    }
    auto &file = files.back();
    file.logical_name = utf8(tokens[3].text());
    const auto current = file.source->position(file.lexer.offset()).line;
    const auto start = tokens[1].spelling.source->position(tokens[1].spelling.begin).line;
    file.lexer.set_location(file.logical_name, line + current - start);
    emit_file(tokens[1], file.logical_name, line);
}

void PreprocessorSession::State::module_context(const std::vector<Token> &tokens) {
    const auto name = tokens[1].text();
    if (tokens.size() < 5 || tokens[3].kind != TokenKind::atom) {
        return;
    }
    if (name != U"module" && name != U"extends") {
        return;
    }
    const std::u32string macro = name == U"module" ? U"MODULE" : U"BASE_MODULE";
    (name == U"module" ? module : base_module) = tokens[3];
    macros.undefined.erase(macro);
    macros.undefined.erase(macro + U"_STRING");
    macros.reserved.insert(macro);
    macros.reserved.insert(macro + U"_STRING");
}

bool PreprocessorSession::State::attribute(const std::vector<Token> &tokens) {
    const auto name = tokens[1].text();
    if (name == U"file") {
        file_mapping(tokens);
        return true;
    }
    module_context(tokens);
    if (name != U"module" && name != U"doc" && name != U"moduledoc") {
        prefix = false;
    }
    return false;
}

void PreprocessorSession::State::ordinary(std::vector<Token> tokens) {
    if (tokens.empty()) {
        return;
    }
    if (tokens.size() >= 2 && syntax(tokens[0], U"-") && tokens[1].kind == TokenKind::atom) {
        if (attribute(tokens)) {
            return;
        }
    } else {
        prefix = false;
    }
    pending.emplace_back(OrdinaryForm{std::move(tokens), feature_snapshot});
}
} // namespace erlang_aot

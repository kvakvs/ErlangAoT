#include <erlang_aot/compiler/preprocessor.hpp>
#include <algorithm>

namespace erlang_aot {
namespace {
// Recognize line-leading attribute syntax without matching literal contents.
bool line_leading(const Token& token)
{
    const auto& span = token.spelling;
    const auto line = span.source->text.rfind(U'\n', span.begin);
    const auto begin = line == std::u32string::npos ? 0 : line + 1;
    const auto prefix = std::u32string_view(span.source->text).substr(begin, span.begin - begin);
    return std::ranges::all_of(prefix, whitespace);
}
// Unary error/warning calls are expressions; leave their interpretation to the parser.
bool structural_directive(DirectiveKind kind)
{
    return kind != DirectiveKind::error && kind != DirectiveKind::warning;
}
// Flag line-leading preprocessing envelopes in a function body, without an AST.
std::optional<Span> misplaced(std::span<const Token> tokens)
{
    if (tokens.front().text() == U"-") { return std::nullopt; }
    const auto arrow = std::ranges::find_if(tokens, [](const Token& token) {
        return token.kind == TokenKind::symbol && token.text() == U"->";
    });
    if (arrow == tokens.end()) { return std::nullopt; }
    auto suffix = tokens.subspan(static_cast<std::size_t>(arrow - tokens.begin()) + 1);
    while (!suffix.empty()) {
        const auto kind = directive_kind(suffix);
        if (kind && structural_directive(*kind) && line_leading(suffix.front())) {
            return suffix.front().spelling;
        }
        suffix = suffix.subspan(1);
    }
    return std::nullopt;
}
} // namespace

PreprocessorSession::PreprocessorSession(SourcePtr source)
{
    includes_.push_back({source, Lexer(std::move(source))});
}
bool PreprocessorSession::failed() const { return failed_; }
PreprocessorEvent PreprocessorSession::error(Diagnostic diagnostic)
{
    failed_ = true;
    return diagnostic;
}
PreprocessorEvent PreprocessorSession::classify(std::vector<Token> tokens)
{
    if (!directive_kind(tokens)) {
        if (const auto location = misplaced(tokens)) {
            return error({DiagnosticCode::misplaced_directive,
                "preprocessing directive must start a separate form", *location, {tokens.front().spelling}});
        }
        return OrdinaryForm{std::move(tokens)};
    }
    auto result = parse_directive(tokens);
    if (auto* diagnostic = std::get_if<Diagnostic>(&result)) { return error(std::move(*diagnostic)); }
    return std::get<Directive>(std::move(result));
}
std::optional<PreprocessorEvent> PreprocessorSession::next()
{
    if (includes_.empty()) { return std::nullopt; }
    auto& frame = includes_.back();
    std::vector<Token> tokens;
    try { tokens = frame.lexer.form(); }
    catch (const LexicalError& diagnostic) {
        frame.lexer.recover_form();
        return error(diagnostic.diagnostic);
    }
    if (tokens.empty()) { includes_.pop_back(); return std::nullopt; }
    if (tokens.back().kind != TokenKind::dot) {
        const auto end = frame.source->text.size();
        return error({DiagnosticCode::missing_terminator, "expected form-ending '.'",
            {frame.source, end, end}, {tokens.front().spelling}});
    }
    return classify(std::move(tokens));
}
} // namespace erlang_aot

#include "expression.hpp"
#include "parsing/operator_info.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {
// Preserve the preprocessor's restricted operator set over shared grammar metadata.
int precedence(const Token &token) {
    const auto info = infix_operator(token, OperatorContext::condition);
    return info ? info->precedence : -1;
}

bool right_associative(const Token &token) {
    const auto info = infix_operator(token, OperatorContext::condition);
    return info && info->associativity == Associativity::right;
}
} // namespace

ExpressionParser::ExpressionParser(std::span<const Token> tokens, std::size_t maximum_depth)
    : input_(tokens, tokens.empty() ? Token{} : tokens.back()), anchor_(tokens.empty() ? Token{} : tokens.back()),
      maximum_depth_(maximum_depth) {}

Token ExpressionParser::consume() {
    if (input_.empty()) {
        pp_fail(DiagnosticCode::invalid_condition, "unexpected end of expression", anchor_);
    }
    const auto result = input_.anchor();
    input_.consume();
    return result;
}

bool ExpressionParser::take(std::u32string_view symbol) {
    if (input_.empty() || !syntax(input_.anchor(), symbol)) {
        return false;
    }
    consume();
    return true;
}

void ExpressionParser::expect(std::u32string_view symbol) {
    if (!take(symbol)) {
        pp_fail(DiagnosticCode::invalid_condition, "expected '" + utf8(symbol) + "'",
                input_.empty() ? anchor_ : input_.anchor());
    }
}

Expr ExpressionParser::parse() {
    auto result = expression();
    if (!input_.empty()) {
        pp_fail(DiagnosticCode::invalid_condition, "unexpected expression token", input_.anchor());
    }
    return result;
}

Expr ExpressionParser::expression(int minimum) {
    if (++depth_ > maximum_depth_) {
        pp_fail(DiagnosticCode::resource_limit, "expression nesting exhausted", anchor_);
    }
    auto left = postfix(primary());
    bool compared = false;
    std::size_t operations = 0;
    while (!input_.empty() && precedence(input_.anchor()) >= minimum) {
        auto operation = consume();
        const auto priority = precedence(operation);
        if (priority == 200 && compared) {
            pp_fail(DiagnosticCode::invalid_condition, "comparisons do not associate", operation);
        }
        compared = priority == 200;
        if (++operations + depth_ > maximum_depth_) {
            pp_fail(DiagnosticCode::resource_limit, "expression nesting exhausted", operation);
        }
        auto right = expression(priority + (right_associative(operation) ? 0 : 1));
        left = {ExprKind::binary, std::move(operation), {std::move(left), std::move(right)}, {}};
    }
    --depth_;
    return left;
}

Expr ExpressionParser::primary() {
    if (input_.empty()) {
        consume();
    }
    if (take(U"(")) {
        auto value = expression();
        expect(U")");
        return value;
    }
    if (take(U"{")) {
        return collection(ExprKind::tuple, U"}");
    }
    if (take(U"[")) {
        return collection(ExprKind::list, U"]");
    }
    if (take(U"<<")) {
        return binary_literal();
    }
    if (take(U"#")) {
        return take(U"{") ? map(ExprKind::map) : record();
    }
    return scalar();
}

// Keep scalar, sigil and function-reference syntax separate from container envelopes.
Expr ExpressionParser::scalar() {
    if (!input_.empty() && input_.anchor().kind == TokenKind::sigil_prefix) {
        return sigil();
    }
    const auto token = consume();
    if (const auto info = prefix_operator(token)) {
        return {ExprKind::unary, token, {expression(info->precedence)}, {}};
    }
    if (syntax(token, U"fun")) {
        return external_fun();
    }
    if (token.kind == TokenKind::atom) {
        return atom_or_call(token);
    }
    return {token.kind == TokenKind::variable ? ExprKind::variable : ExprKind::literal, token, {}, {}};
}

// OTP's standard sigils are string lists (s/S) or UTF-8 binaries (b/B/empty).
Expr ExpressionParser::sigil() {
    const auto prefix = consume();
    auto text = consume();
    const auto suffix = consume();
    if (text.kind != TokenKind::string || suffix.kind != TokenKind::sigil_suffix || !suffix.text().empty()) {
        pp_fail(DiagnosticCode::invalid_condition, "invalid string sigil", prefix);
    }
    if (prefix.text() == U"s" || prefix.text() == U"S") {
        return {ExprKind::literal, text, {}, {}};
    }
    if (!prefix.text().empty() && prefix.text() != U"b" && prefix.text() != U"B") {
        pp_fail(DiagnosticCode::invalid_condition, "unknown string sigil", prefix);
    }
    Expr result{ExprKind::bits, prefix, {}, {}};
    for (const unsigned char byte : utf8(text.text())) {
        Expr literal{ExprKind::literal, generated(text, TokenKind::integer, Integer{std::to_string(byte)}), {}, {}};
        result.children.push_back({ExprKind::segment, text, {std::move(literal)}, {}});
    }
    return result;
}

// Preserve record syntax for guard validation; preprocessing does not expand record declarations.
Expr ExpressionParser::record(std::optional<Expr> base) {
    const auto name = consume();
    if (name.kind != TokenKind::atom) {
        pp_fail(DiagnosticCode::invalid_condition, "expected record name", name);
    }
    Expr result{ExprKind::record, name, {}, {}};
    if (base) {
        result.children.push_back(std::move(*base));
    }
    if (take(U".")) {
        const auto field = consume();
        if (field.kind != TokenKind::atom) {
            pp_fail(DiagnosticCode::invalid_condition, "expected record field", field);
        }
        return result;
    }
    expect(U"{");
    result.modifiers.push_back(name);
    if (take(U"}")) {
        return result;
    }
    do {
        const auto field = consume();
        if (field.kind != TokenKind::atom && field.text() != U"_") {
            pp_fail(DiagnosticCode::invalid_condition, "expected record field", field);
        }
        expect(U"=");
        result.children.push_back(expression());
    } while (take(U","));
    expect(U"}");
    return result;
}

// External fun references are literal terms; anonymous/local fun expressions are not guard terms.
Expr ExpressionParser::external_fun() {
    const auto module = consume();
    expect(U":");
    const auto name = consume();
    expect(U"/");
    const auto arity = consume();
    if (module.kind != TokenKind::atom || name.kind != TokenKind::atom || arity.kind != TokenKind::integer) {
        pp_fail(DiagnosticCode::invalid_condition, "expected external function reference", name);
    }
    return {
        ExprKind::external_fun, module, {{ExprKind::literal, name, {}, {}}, {ExprKind::literal, arity, {}, {}}}, {}};
}

Expr ExpressionParser::atom_or_call(Token name) {
    const bool qualified = take(U":");
    if (qualified) {
        const auto function = consume();
        if (name.text() != U"erlang" || function.kind != TokenKind::atom) {
            pp_fail(DiagnosticCode::invalid_condition, "only erlang guard calls are permitted", name);
        }
        name = function;
    }
    if (!take(U"(")) {
        if (qualified) {
            pp_fail(DiagnosticCode::invalid_condition, "expected guard call arguments", name);
        }
        return {ExprKind::literal, std::move(name), {}, {}};
    }
    auto result = collection(ExprKind::call, U")");
    result.token = std::move(name);
    if (qualified) {
        result.modifiers.push_back(result.token);
    }
    return result;
}

Expr ExpressionParser::collection(ExprKind kind, std::u32string_view closing) {
    Expr result{kind, anchor_, {}, {}};
    if (take(closing)) {
        return result;
    }
    do {
        result.children.push_back(expression());
    } while (take(U","));
    if (kind == ExprKind::list && take(U"|")) {
        result.modifiers.push_back(generated(anchor_, TokenKind::symbol, std::u32string(U"|")));
        result.children.push_back(expression());
    }
    expect(closing);
    return result;
}

Expr ExpressionParser::postfix(Expr base) {
    while (take(U"#")) {
        base = take(U"{") ? map(ExprKind::map_update, std::move(base)) : record(std::move(base));
    }
    // Adjacent Erlang strings concatenate before parsing, including macro-produced strings.
    while (!input_.empty() && base.token.kind == TokenKind::string && input_.anchor().kind == TokenKind::string) {
        std::get<std::u32string>(base.token.value) += consume().text();
    }
    return base;
}

Expr ExpressionParser::map(ExprKind kind, std::optional<Expr> base) {
    Expr result{kind, anchor_, {}, {}};
    if (base) {
        result.children.push_back(std::move(*base));
    }
    if (take(U"}")) {
        return result;
    }
    do {
        result.children.push_back(expression());
        auto operation = consume();
        if (!syntax(operation, U"=>") && !syntax(operation, U":=")) {
            pp_fail(DiagnosticCode::invalid_condition, "expected map association", operation);
        }
        result.modifiers.push_back(operation);
        result.children.push_back(expression());
    } while (take(U","));
    expect(U"}");
    return result;
}

Expr ExpressionParser::binary_literal() {
    Expr result{ExprKind::bits, anchor_, {}, {}};
    if (take(U">>")) {
        return result;
    }
    do {
        result.children.push_back(segment());
    } while (take(U","));
    expect(U">>");
    return result;
}

Expr ExpressionParser::segment() {
    // Bare segment values are primaries; operators in size/value require parentheses.
    Expr result{ExprKind::segment, anchor_, {primary()}, {}};
    if (take(U":")) {
        result.children.push_back(primary());
    }
    if (take(U"/")) {
        do {
            result.modifiers.push_back(consume());
            if (take(U":")) {
                result.modifiers.push_back(consume());
            }
        } while (take(U"-"));
    }
    return result;
}
} // namespace erlang_aot

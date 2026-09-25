#include "delimiters.hpp"
#include <map>

namespace erlang_aot {
bool Delimiters::boundary(const Token &token) {
    if (!stack_.empty() && stack_.back().close == U"fun") {
        if (syntax(token, U"->") || syntax(token, U"when")) {
            stack_.back().close = U"end";
        } else if (syntax(token, U",") || syntax(token, U")")) {
            stack_.pop_back();
        }
    }
    return stack_.empty() && (syntax(token, U",") || syntax(token, U")"));
}

void Delimiters::consume(const std::span<const Token> input) {
    const auto &token = input.front();
    if (token.kind != TokenKind::symbol && token.kind != TokenKind::keyword) {
        return;
    }
    if (!stack_.empty() && token.text() == stack_.back().close) {
        stack_.pop_back();
        return;
    }
    static const std::map<std::u32string_view, std::u32string_view> pairs{
        {U"(", U")"},       {U"[", U"]"},       {U"{", U"}"},      {U"<<", U">>"},
        {U"begin", U"end"}, {U"if", U"end"},    {U"case", U"end"}, {U"receive", U"end"},
        {U"try", U"end"},   {U"maybe", U"end"}, {U"cond", U"end"}};
    if (const auto found = pairs.find(token.text()); found != pairs.end()) {
        stack_.push_back({.close = found->second, .open = &token});
    }
    if (token.text() == U"fun") {
        fun(input);
    }
}

void Delimiters::fun(std::span<const Token> input) {
    if (input.size() > 1 && syntax(input[1], U"(")) {
        stack_.push_back({.close = U"fun", .open = &input.front()});
    }
    if (input.size() > 2 && input[1].kind == TokenKind::variable && syntax(input[2], U"(")) {
        stack_.push_back({.close = U"end", .open = &input.front()});
    }
}

const Token *Delimiters::opener() const { return stack_.empty() ? nullptr : stack_.back().open; }
} // namespace erlang_aot

#include "macros.hpp"
#include "token_utils.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {
// Track the ambiguous fun(...) prefix until an arrow/guard or enclosing delimiter resolves it.
class Balance {
  public:
    // Matching delimiters; "fun" is the unresolved fun-type/body sentinel.
    std::vector<std::u32string_view> closers;

    bool boundary(const Token &token) {
        if (!closers.empty() && closers.back() == U"fun") {
            if (syntax(token, U"->") || syntax(token, U"when")) {
                closers.back() = U"end";
            } else if (syntax(token, U",") || syntax(token, U")")) {
                closers.pop_back();
            }
        }
        return closers.empty() && (syntax(token, U",") || syntax(token, U")"));
    }

    // Handle delimiters and keyword constructs, leaving quoted words untouched.
    void consume(std::span<const Token> input) {
        const auto &token = input.front();
        if (token.kind != TokenKind::symbol && token.kind != TokenKind::keyword) {
            return;
        }
        if (!closers.empty() && token.text() == closers.back()) {
            closers.pop_back();
            return;
        }
        static const std::map<std::u32string_view, std::u32string_view> pairs{
            {U"(", U")"},       {U"[", U"]"},       {U"{", U"}"},      {U"<<", U">>"},
            {U"begin", U"end"}, {U"if", U"end"},    {U"case", U"end"}, {U"receive", U"end"},
            {U"try", U"end"},   {U"maybe", U"end"}, {U"cond", U"end"}};
        if (const auto found = pairs.find(token.text()); found != pairs.end()) {
            closers.push_back(found->second);
        }
        if (token.text() == U"fun") {
            fun(input);
        }
    }

  private:
    // Named funs require end; fun references do not push any delimiter.
    void fun(std::span<const Token> input) {
        if (input.size() > 1 && syntax(input[1], U"(")) {
            closers.push_back(U"fun");
        }
        if (input.size() > 2 && input[1].kind == TokenKind::variable && syntax(input[2], U"(")) {
            closers.push_back(U"end");
        }
    }
};

// Publish an argument at a top-level separator, retaining the special zero-arity case.
void finish_argument(Arguments &result, std::vector<Token> &current, const Token &token) {
    if (!current.empty()) {
        result.values.push_back(std::move(current));
        current.clear();
    } else if (!result.values.empty() || syntax(token, U",")) {
        pp_fail(DiagnosticCode::macro_arguments, "empty macro argument", token);
    }
}
} // namespace

Arguments collect_arguments(std::span<const Token> input, const Token &call) {
    Arguments result;
    if (input.empty() || !syntax(input.front(), U"(")) {
        return result;
    }
    Balance balance;
    std::vector<Token> current;
    for (std::size_t index = 1; index < input.size(); ++index) {
        const auto &token = input[index];
        if (!balance.boundary(token)) {
            balance.consume(input.subspan(index));
            current.push_back(token);
            continue;
        }
        finish_argument(result, current, token);
        if (syntax(token, U")")) {
            result.consumed = index + 1;
            return result;
        }
    }
    pp_fail(DiagnosticCode::macro_arguments, "unterminated macro arguments", call);
}
} // namespace erlang_aot

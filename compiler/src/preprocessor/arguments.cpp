#include "macros.hpp"
#include "parsing/delimiters.hpp"
#include "token_utils.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {

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
    Delimiters balance;
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

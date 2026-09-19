#include "operator_info.hpp"
#include "token_syntax.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace erlang_aot {
namespace {
struct Entry {
    // Share expression binding metadata while explicitly restricting condition/type use.
    OperatorInfo info;
    bool condition;
    bool type;
};

constexpr auto left = Associativity::left;
constexpr auto right = Associativity::right;
constexpr auto none = Associativity::none;
using enum ast::BinaryOperator;
constexpr Entry entries[]{{{U":", 800, none}, false, false},
                          {{U"=", 100, right}, false, false},
                          {{U"!", 100, right, send}, false, false},
                          {{U"orelse", 150, right, or_else}, true, false},
                          {{U"andalso", 160, right, and_also}, true, false},
                          {{U"==", 200, none, equal}, true, false},
                          {{U"/=", 200, none, not_equal}, true, false},
                          {{U"=<", 200, none, less_equal}, true, false},
                          {{U"<", 200, none, less}, true, false},
                          {{U">=", 200, none, greater_equal}, true, false},
                          {{U">", 200, none, greater}, true, false},
                          {{U"=:=", 200, none, exact_equal}, true, false},
                          {{U"=/=", 200, none, exact_not_equal}, true, false},
                          {{U"++", 300, right, append}, true, false},
                          {{U"--", 300, right, subtract_list}, true, false},
                          {{U"+", 400, left, add}, true, true},
                          {{U"-", 400, left, subtract}, true, true},
                          {{U"bor", 400, left, bit_or}, true, true},
                          {{U"bxor", 400, left, bit_xor}, true, true},
                          {{U"bsl", 400, left, shift_left}, true, true},
                          {{U"bsr", 400, left, shift_right}, true, true},
                          {{U"or", 400, left, logical_or}, true, true},
                          {{U"xor", 400, left, logical_xor}, true, true},
                          {{U"*", 500, left, multiply}, true, true},
                          {{U"/", 500, left, divide}, true, true},
                          {{U"div", 500, left, integer_divide}, true, true},
                          {{U"rem", 500, left, remainder}, true, true},
                          {{U"band", 500, left, bit_and}, true, true},
                          {{U"and", 500, left, logical_and}, true, true}};
constexpr OperatorInfo type_entries[]{{U"::", 150, right}, {U"|", 170, left}, {U"..", 200, none}};
constexpr std::pair<std::u32string_view, ast::UnaryOperator> prefixes[]{{U"+", ast::UnaryOperator::positive},
                                                                        {U"-", ast::UnaryOperator::negative},
                                                                        {U"bnot", ast::UnaryOperator::bit_not},
                                                                        {U"not", ast::UnaryOperator::logical_not}};

// Apply the grammar context without changing a shared operator's binding strength.
bool allowed(const Entry &entry, OperatorContext context) {
    if (context == OperatorContext::pattern) {
        const auto name = entry.info.spelling;
        return name != U"!" && name != U"orelse" && name != U"andalso" && name != U":";
    }
    if (context == OperatorContext::condition) {
        return entry.condition;
    }
    return context != OperatorContext::type || entry.type;
}
} // namespace

std::optional<PrefixOperatorInfo> prefix_operator(const Token &token) {
    for (const auto &[spelling, operation] : prefixes) {
        if (syntax(token, spelling)) {
            return PrefixOperatorInfo{operation};
        }
    }
    return std::nullopt;
}

std::optional<OperatorInfo> call_operator(const Token &token) {
    if (syntax(token, U"(")) {
        return OperatorInfo{U"(", 750, Associativity::left};
    }
    return std::nullopt;
}

std::optional<OperatorInfo> infix_operator(const Token &token, OperatorContext context) {
    if (token.kind != TokenKind::keyword && token.kind != TokenKind::symbol) {
        return std::nullopt;
    }
    const auto name = token.text();
    if (context == OperatorContext::type) {
        const auto *found = std::ranges::find(type_entries, name, &OperatorInfo::spelling);
        if (found != std::end(type_entries)) {
            return *found;
        }
    }
    const auto *found =
        std::ranges::find_if(entries, [name](const Entry &entry) { return entry.info.spelling == name; });
    if (found == std::end(entries) || !allowed(*found, context)) {
        return std::nullopt;
    }
    return found->info;
}

std::u32string_view operator_spelling(ast::BinaryOperator operation) {
    const auto *found =
        std::ranges::find_if(entries, [operation](const Entry &entry) { return entry.info.operation == operation; });
    if (found == std::end(entries)) {
        throw std::invalid_argument("unknown binary operator");
    }
    return found->info.spelling;
}

std::u32string_view operator_spelling(ast::UnaryOperator operation) {
    const auto *found =
        std::ranges::find_if(prefixes, [operation](const auto &entry) { return entry.second == operation; });
    if (found == std::end(prefixes)) {
        throw std::invalid_argument("unknown unary operator");
    }
    return found->first;
}
} // namespace erlang_aot

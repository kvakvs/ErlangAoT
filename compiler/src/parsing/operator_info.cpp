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
constexpr Entry entries[]{
    {.info = {.spelling = U":", .precedence = 800, .associativity = none}, .condition = false, .type = false},
                          {.info = {.spelling = U"=", .precedence = 100, .associativity = right}, .condition = false, .type = false},
                          {.info = {.spelling = U"!", .precedence = 100, .associativity = right, .operation = send}, .condition = false, .type = false},
                          {.info = {.spelling = U"orelse", .precedence = 150, .associativity = right, .operation = or_else}, .condition = true, .type = false},
                          {.info = {.spelling = U"andalso", .precedence = 160, .associativity = right, .operation = and_also}, .condition = true, .type = false},
                          {.info = {.spelling = U"==", .precedence = 200, .associativity = none, .operation = equal}, .condition = true, .type = false},
                          {.info = {.spelling = U"/=", .precedence = 200, .associativity = none, .operation = not_equal}, .condition = true, .type = false},
                          {.info = {.spelling = U"=<", .precedence = 200, .associativity = none, .operation = less_equal}, .condition = true, .type = false},
                          {.info = {.spelling = U"<", .precedence = 200, .associativity = none, .operation = less}, .condition = true, .type = false},
                          {.info = {.spelling = U">=", .precedence = 200, .associativity = none, .operation = greater_equal}, .condition = true, .type = false},
                          {.info = {.spelling = U">", .precedence = 200, .associativity = none, .operation = greater}, .condition = true, .type = false},
                          {.info = {.spelling = U"=:=", .precedence = 200, .associativity = none, .operation = exact_equal}, .condition = true, .type = false},
                          {.info = {.spelling = U"=/=", .precedence = 200, .associativity = none, .operation = exact_not_equal}, .condition = true, .type = false},
                          {.info = {.spelling = U"++", .precedence = 300, .associativity = right, .operation = append}, .condition = true, .type = false},
                          {.info = {.spelling = U"--", .precedence = 300, .associativity = right, .operation = subtract_list}, .condition = true, .type = false},
                          {.info = {.spelling = U"+", .precedence = 400, .associativity = left, .operation = add}, .condition = true, .type = true},
                          {.info = {.spelling = U"-", .precedence = 400, .associativity = left, .operation = subtract}, .condition = true, .type = true},
                          {.info = {.spelling = U"bor", .precedence = 400, .associativity = left, .operation = bit_or}, .condition = true, .type = true},
                          {.info = {.spelling = U"bxor", .precedence = 400, .associativity = left, .operation = bit_xor}, .condition = true, .type = true},
                          {.info = {.spelling = U"bsl", .precedence = 400, .associativity = left, .operation = shift_left}, .condition = true, .type = true},
                          {.info = {.spelling = U"bsr", .precedence = 400, .associativity = left, .operation = shift_right}, .condition = true, .type = true},
                          {.info = {.spelling = U"or", .precedence = 400, .associativity = left, .operation = logical_or}, .condition = true, .type = true},
                          {.info = {.spelling = U"xor", .precedence = 400, .associativity = left, .operation = logical_xor}, .condition = true, .type = true},
                          {.info = {.spelling = U"*", .precedence = 500, .associativity = left, .operation = multiply}, .condition = true, .type = true},
                          {.info = {.spelling = U"/", .precedence = 500, .associativity = left, .operation = divide}, .condition = true, .type = true},
                          {.info = {.spelling = U"div", .precedence = 500, .associativity = left, .operation = integer_divide}, .condition = true, .type = true},
                          {.info = {.spelling = U"rem", .precedence = 500, .associativity = left, .operation = remainder}, .condition = true, .type = true},
                          {.info = {.spelling = U"band", .precedence = 500, .associativity = left, .operation = bit_and}, .condition = true, .type = true},
                          {.info = {.spelling = U"and", .precedence = 500, .associativity = left, .operation = logical_and}, .condition = true, .type = true}};
constexpr OperatorInfo type_entries[]{{.spelling = U"::", .precedence = 150, .associativity = right}, {.spelling = U"|", .precedence = 170, .associativity = left}, {.spelling = U"..", .precedence = 200, .associativity = none}};
constexpr std::pair<std::u32string_view, ast::UnaryOperator> prefixes[]{{U"+", ast::UnaryOperator::positive},
                                                                        {U"-", ast::UnaryOperator::negative},
                                                                        {U"bnot", ast::UnaryOperator::bit_not},
                                                                        {U"not", ast::UnaryOperator::logical_not}};

// Apply the grammar context without changing a shared operator's binding strength.
bool allowed(const Entry &entry, const OperatorContext context) {
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
            return PrefixOperatorInfo{.operation = operation};
        }
    }
    return std::nullopt;
}

std::optional<OperatorInfo> call_operator(const Token &token) {
    if (syntax(token, U"(")) {
        return OperatorInfo{.spelling = U"(", .precedence = 750, .associativity = Associativity::left};
    }
    return std::nullopt;
}

std::optional<OperatorInfo> infix_operator(const Token &token, const OperatorContext context) {
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

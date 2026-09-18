#include "operator_info.hpp"
#include <algorithm>
#include <array>

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
constexpr Entry entries[]{
    {{U"=", 100, right}, false, false},      {{U"!", 100, right}, false, false}, {{U"orelse", 150, right}, true, false},
    {{U"andalso", 160, right}, true, false}, {{U"==", 200, none}, true, false},  {{U"/=", 200, none}, true, false},
    {{U"=<", 200, none}, true, false},       {{U"<", 200, none}, true, false},   {{U">=", 200, none}, true, false},
    {{U">", 200, none}, true, false},        {{U"=:=", 200, none}, true, false}, {{U"=/=", 200, none}, true, false},
    {{U"++", 300, right}, true, false},      {{U"--", 300, right}, true, false}, {{U"+", 400, left}, true, true},
    {{U"-", 400, left}, true, true},         {{U"bor", 400, left}, true, true},  {{U"bxor", 400, left}, true, true},
    {{U"bsl", 400, left}, true, true},       {{U"bsr", 400, left}, true, true},  {{U"or", 400, left}, true, true},
    {{U"xor", 400, left}, true, true},       {{U"*", 500, left}, true, true},    {{U"/", 500, left}, true, true},
    {{U"div", 500, left}, true, true},       {{U"rem", 500, left}, true, true},  {{U"band", 500, left}, true, true},
    {{U"and", 500, left}, true, true}};
constexpr OperatorInfo type_entries[]{{U"::", 150, right}, {U"|", 170, left}, {U"..", 200, none}};

// Apply the grammar context without changing a shared operator's binding strength.
bool allowed(const Entry &entry, OperatorContext context) {
    if (context == OperatorContext::condition) {
        return entry.condition;
    }
    return context != OperatorContext::type || entry.type;
}
} // namespace

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
} // namespace erlang_aot

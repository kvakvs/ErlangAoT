#include "forms.hpp"

namespace erlang_aot {
// Parse ordered segments after the opening delimiter, including the empty binary.
ast::ExprValue FormParser::binary(bool comprehension) {
    std::vector<ast::BinarySegment> segments;
    if (!cursor_.take_syntax(U">>")) {
        do {
            segments.push_back(binary_segment());
        } while (cursor_.take_syntax(U","));
        if (cursor_.take_syntax(U"||")) {
            require_comprehension(comprehension);
            auto value = binary_template(segments);
            auto items = qualifiers();
            expect(U">>");
            return ast::BinaryComprehension{std::move(value), std::move(items)};
        }
        expect(U">>");
    }
    return ast::Bitstring{std::move(segments)};
}

// A segment owns explicit defaults and modifier order while deferring all bit-type validation.
ast::BinarySegment FormParser::binary_segment() {
    const auto begin = cursor_.offset();
    auto value = bit_value();
    std::optional<ast::ExprId> size;
    if (cursor_.take_syntax(U":")) {
        size = bit_primary();
    }
    auto modifiers = binary_modifiers();
    return {std::move(value), std::move(size), std::move(modifiers), builder_.source(begin, cursor_.offset(), begin)};
}

// bit_expr allows one prefix operator on expr_max; repeated prefixes need parentheses.
ast::ExprId FormParser::bit_value() {
    const auto begin = cursor_.offset();
    if (!cursor_.empty()) {
        if (const auto operation = prefix_operator(cursor_.anchor())) {
            cursor_.consume();
            auto operand = bit_primary();
            return make(ast::UnaryExpression{operation->operation, std::move(operand)}, begin, begin);
        }
    }
    return bit_primary();
}

// expr_max excludes bare calls/maps/records/operators but grouping re-enters general expr.
ast::ExprId FormParser::bit_primary() {
    enter();
    const auto begin = cursor_.offset();
    auto value = primary(OperatorContext::expression);
    auto result = make(std::move(value), begin, begin);
    --depth_;
    return result;
}

// An omitted slash denotes default types; an explicit slash requires at least one modifier.
std::optional<std::vector<ast::BinaryModifier>> FormParser::binary_modifiers() {
    if (!cursor_.take_syntax(U"/")) {
        return std::nullopt;
    }
    std::vector<ast::BinaryModifier> modifiers;
    do {
        modifiers.push_back(binary_modifier());
    } while (cursor_.take_syntax(U"-"));
    return modifiers;
}

// Type parameters retain arbitrary integer precision, without enforcing unit ranges or names.
ast::BinaryModifier FormParser::binary_modifier() {
    const auto begin = cursor_.offset();
    ast::Atom name{value<std::u32string>(category(TokenKind::atom, "binary type atom"))};
    std::optional<Integer> parameter;
    if (cursor_.take_syntax(U":")) {
        parameter = value<Integer>(category(TokenKind::integer, "type parameter integer"));
    }
    return {std::move(name), std::move(parameter), builder_.source(begin, cursor_.offset(), begin)};
}

// OTP lowers binary sigils to one decoded string segment with an implicit UTF-8 modifier.
ast::Bitstring FormParser::binary_sigil(std::u32string content, std::size_t begin) {
    node();
    const auto string_source = builder_.source(begin + 1, begin + 2, begin + 1);
    auto string = builder_.expression(ast::StringLiteral{std::move(content)}, string_source);
    std::vector<ast::BinaryModifier> modifiers{{{U"utf8"}, {}, builder_.source(begin, begin + 1, begin)}};
    return {{{std::move(string), {}, std::move(modifiers), builder_.source(begin, cursor_.offset(), begin + 1)}}};
}
} // namespace erlang_aot

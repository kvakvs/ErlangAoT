#include "forms.hpp"

namespace erlang_aot {
void FormParser::require_comprehension(bool allowed) const {
    if (!allowed) {
        fail(DiagnosticCode::parser_syntax, "comprehension is not allowed in this grammar context");
    }
}

ast::ExprValue FormParser::maximum_hash() {
    auto value = hash();
    if (!std::holds_alternative<ast::MapComprehension>(value)) {
        fail(DiagnosticCode::parser_syntax, "only map comprehensions are expr_max hash syntax");
    }
    return value;
}

ast::PatternSyntaxId FormParser::candidate(ast::ExprId expression) {
    const auto source = builder_.view().expression(expression).source;
    node();
    return builder_.pattern(ast::PatternCandidate{std::move(expression)}, source);
}

std::vector<ast::ComprehensionQualifier> FormParser::qualifiers() {
    std::vector<ast::ComprehensionQualifier> result;
    do {
        result.push_back(qualifier_group());
    } while (cursor_.take_syntax(U","));
    return result;
}

ast::ComprehensionQualifier FormParser::qualifier_group() {
    const auto begin = cursor_.offset();
    auto first = qualifier();
    if (!cursor_.take_syntax(U"&&")) {
        return first;
    }
    std::vector<ast::Qualifier> items;
    items.push_back(std::move(first));
    do {
        items.push_back(qualifier());
    } while (cursor_.take_syntax(U"&&"));
    return ast::ZippedQualifier{std::move(items), builder_.source(begin, cursor_.offset(), begin)};
}

ast::Qualifier FormParser::qualifier() {
    const auto begin = cursor_.offset();
    auto left = expression();
    if (cursor_.take_syntax(U":=")) {
        return map_generator(std::move(left), begin);
    }
    return generator(std::move(left), begin);
}

ast::Qualifier FormParser::map_generator(ast::ExprId key, std::size_t begin) {
    auto value = expression();
    const auto anchor = cursor_.offset();
    const bool strict = cursor_.take_syntax(U"<:-");
    if (!strict) {
        expect(U"<-");
    }
    auto input = expression();
    return {ast::MapGenerator{candidate(std::move(key)), candidate(std::move(value)), std::move(input), strict},
            builder_.source(begin, cursor_.offset(), anchor)};
}

std::optional<GeneratorOperator> FormParser::generator_operator() const {
    if (syntax(cursor_.anchor(), U"<-")) {
        return GeneratorOperator{false, false};
    }
    if (syntax(cursor_.anchor(), U"<:-")) {
        return GeneratorOperator{false, true};
    }
    if (syntax(cursor_.anchor(), U"<=")) {
        return GeneratorOperator{true, false};
    }
    if (syntax(cursor_.anchor(), U"<:=")) {
        return GeneratorOperator{true, true};
    }
    return std::nullopt;
}

void FormParser::require_binary_generator(const ast::ExprId &pattern, std::size_t begin) {
    if (!std::holds_alternative<ast::Bitstring>(builder_.view().expression(pattern).value)) {
        fail(DiagnosticCode::parser_syntax, "binary generator requires binary syntax");
    }
    // Sigils normalize to bitstrings but are not the grammar's binary production.
    const auto checkpoint = cursor_.checkpoint();
    cursor_.restore(begin);
    const bool opening = syntax(cursor_.anchor(), U"<<");
    cursor_.restore(checkpoint);
    if (!opening) {
        fail(DiagnosticCode::parser_syntax, "binary generator requires a binary literal");
    }
}

ast::ExprId FormParser::binary_template(const std::vector<ast::BinarySegment> &segments) const {
    if (segments.size() != 1) {
        fail(DiagnosticCode::parser_syntax, "binary comprehension requires one template");
    }
    const auto &first = segments.front();
    if (first.size || first.modifiers ||
        std::holds_alternative<ast::UnaryExpression>(builder_.view().expression(first.value).value)) {
        fail(DiagnosticCode::parser_syntax, "binary comprehension template requires expr_max");
    }
    return first.value;
}

ast::Qualifier FormParser::generator(ast::ExprId left, std::size_t begin) {
    const auto anchor = cursor_.offset();
    const auto operation = generator_operator();
    if (!operation) {
        return {ast::FilterQualifier{std::move(left)}, builder_.source(begin, cursor_.offset(), begin)};
    }
    if (operation->binary) {
        require_binary_generator(left, begin);
    }
    cursor_.consume();
    auto input = expression();
    auto pattern = candidate(std::move(left));
    const auto source = builder_.source(begin, cursor_.offset(), anchor);
    if (operation->binary) {
        return {ast::BinaryGenerator{std::move(pattern), std::move(input), operation->strict}, source};
    }
    return {ast::ListGenerator{std::move(pattern), std::move(input), operation->strict}, source};
}
} // namespace erlang_aot

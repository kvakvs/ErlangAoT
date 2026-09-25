#include "forms.hpp"

namespace erlang_aot {
// Consume map fields as general expressions even when the enclosing root is a pattern.
ast::ExprValue FormParser::map(std::optional<ast::ExprId> base, const bool comprehension) {
    expect(U"{");
    std::vector<ast::MapField> fields;
    if (!cursor_.take_syntax(U"}")) {
        do {
            fields.push_back(map_field());
        } while (cursor_.take_syntax(U","));
        if (cursor_.take_syntax(U"||")) {
            require_comprehension(comprehension && !base);
            auto items = qualifiers();
            expect(U"}");
            return ast::MapComprehension{.templates = std::move(fields), .qualifiers = std::move(items)};
        }
        expect(U"}");
    }
    return ast::MapExpression{.base = std::move(base), .fields = std::move(fields)};
}

// Anchor each field at its association/exact operator, preserving the complete field extent.
ast::MapField FormParser::map_field() {
    const auto begin = cursor_.offset();
    auto key = expression();
    const auto anchor = cursor_.offset();
    auto kind = ast::MapFieldKind::associate;
    if (!cursor_.take_syntax(U"=>")) {
        expect(U":=");
        kind = ast::MapFieldKind::exact;
    }
    auto value = expression();
    return {.kind = kind,
            .key = std::move(key),
            .value = std::move(value),
            .source = builder_.source(begin, cursor_.offset(), anchor)};
}
} // namespace erlang_aot

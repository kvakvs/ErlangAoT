#include "forms.hpp"

namespace erlang_aot {
// Consume map fields as general expressions even when the enclosing root is a pattern.
ast::MapExpression FormParser::map(std::optional<ast::ExprId> base) {
    expect(U"{");
    std::vector<ast::MapField> fields;
    if (!cursor_.take_syntax(U"}")) {
        do {
            fields.push_back(map_field());
        } while (cursor_.take_syntax(U","));
        expect(U"}");
    }
    return {std::move(base), std::move(fields)};
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
    return {kind, std::move(key), std::move(value), builder_.source(begin, cursor_.offset(), anchor)};
}
} // namespace erlang_aot

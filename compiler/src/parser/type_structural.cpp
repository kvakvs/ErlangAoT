#include "forms.hpp"

namespace erlang_aot {
ast::TypeValue FormParser::hash_type() {
    if (cursor_.take_syntax(U"{")) {
        ast::MapType result{.any = false, .fields = {}};
        if (cursor_.take_syntax(U"}")) {
            return result;
        }
        do {
            result.fields.push_back(map_type_field());
        } while (cursor_.take_syntax(U","));
        expect(U"}");
        return result;
    }
    auto name = ast::Atom{value<std::u32string>(category(TokenKind::atom, "record type name"))};
    std::optional<ast::Atom> module;
    if (cursor_.take_syntax(U":")) {
        module = std::move(name);
        name = record_name();
    }
    expect(U"{");
    ast::RecordType result{.module = std::move(module), .name = std::move(name), .fields = {}};
    if (cursor_.take_syntax(U"}")) {
        return result;
    }
    do {
        result.fields.push_back(record_type_field());
    } while (cursor_.take_syntax(U","));
    expect(U"}");
    return result;
}

ast::MapTypeField FormParser::map_type_field() {
    const auto begin = cursor_.offset();
    auto key = top_type();
    const auto anchor = cursor_.offset();
    auto kind = ast::MapFieldKind::associate;
    if (!cursor_.take_syntax(U"=>")) {
        expect(U":=");
        kind = ast::MapFieldKind::exact;
    }
    auto type = top_type();
    return {.kind = kind,
            .key = std::move(key),
            .value = std::move(type),
            .source = builder_.source(begin, cursor_.offset(), anchor)};
}

ast::RecordTypeField FormParser::record_type_field() {
    const auto begin = cursor_.offset();
    auto name = ast::Atom{value<std::u32string>(category(TokenKind::atom, "record field name"))};
    expect(U"::");
    auto type = top_type();
    return {
        .name = std::move(name), .type = std::move(type), .source = builder_.source(begin, cursor_.offset(), begin)};
}

std::pair<bool, ast::TypeId> FormParser::binary_type_part() {
    const auto &variable = category(TokenKind::variable, "binary type placeholder");
    if (variable.text() != U"_") {
        fail(DiagnosticCode::parser_syntax, "bad binary type variable");
    }
    expect(U":");
    const auto *next = cursor_.peek(1);
    const bool unit = cursor_.anchor().kind == TokenKind::variable && next && syntax(*next, U"*");
    if (unit) {
        if (cursor_.consume()->text() != U"_") {
            fail(DiagnosticCode::parser_syntax, "bad binary unit variable");
        }
        expect(U"*");
    }
    return {unit, type_expression()};
}

ast::BitstringType FormParser::bitstring_type() {
    ast::BitstringType result;
    if (cursor_.take_syntax(U">>")) {
        return result;
    }
    auto [unit, type] = binary_type_part();
    if (unit) {
        result.unit = std::move(type);
    } else {
        result.base = std::move(type);
    }
    if (cursor_.take_syntax(U",")) {
        if (unit) {
            fail(DiagnosticCode::parser_syntax, "binary base must precede unit");
        }
        auto [second_unit, second] = binary_type_part();
        if (!second_unit) {
            fail(DiagnosticCode::parser_syntax, "expected binary unit type");
        }
        result.unit = std::move(second);
    }
    expect(U">>");
    return result;
}
} // namespace erlang_aot

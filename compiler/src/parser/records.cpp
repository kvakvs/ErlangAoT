#include "forms.hpp"

namespace erlang_aot {
// Convert names only in record_name positions; never change the scanner's classification.
ast::Atom FormParser::record_name() {
    const auto &token = cursor_.anchor();
    const bool named = token.kind == TokenKind::atom || token.kind == TokenKind::variable;
    if (cursor_.empty() || (!named && token.kind != TokenKind::keyword)) {
        fail(DiagnosticCode::parser_syntax, "expected record name");
    }
    cursor_.consume();
    return {value<std::u32string>(token)};
}

// Retain the module/name distinction; qualified module positions accept atoms only.
ast::RecordIdentity FormParser::record_identity() {
    const auto begin = cursor_.offset();
    if (cursor_.take_syntax(U"#_")) {
        return {ast::InferredRecordName{}, builder_.source(begin, cursor_.offset(), begin)};
    }
    expect(U"#");
    const auto start = cursor_.offset();
    const auto kind = cursor_.anchor().kind;
    auto first = record_name();
    if (cursor_.take_syntax(U":")) {
        if (kind != TokenKind::atom)
            fail(DiagnosticCode::parser_syntax, "record module must be an atom");
        auto name = record_name();
        return {ast::QualifiedRecordName{std::move(first), std::move(name)},
                builder_.source(start, cursor_.offset(), start)};
    }
    return {ast::UnresolvedRecordName{std::move(first)}, builder_.source(start, cursor_.offset(), start)};
}

// Index syntax has stricter names than construction or access syntax.
ast::ExprValue FormParser::record_access(std::optional<ast::ExprId> base, ast::RecordIdentity identity) {
    const auto field_begin = cursor_.offset();
    ast::Atom field{value<std::u32string>(category(TokenKind::atom, "record field atom"))};
    auto field_source = builder_.source(field_begin, cursor_.offset(), field_begin);
    if (base) {
        return ast::RecordAccess{std::move(*base), std::move(identity), std::move(field), std::move(field_source)};
    }
    const auto *local = std::get_if<ast::UnresolvedRecordName>(&identity.value);
    if (!local)
        fail(DiagnosticCode::parser_syntax, "record index requires an unqualified atom name");
    return ast::RecordIndex{local->name, std::move(field), std::move(identity.source), std::move(field_source)};
}

// Choose construction/update or field/index syntax without resolving a record definition.
ast::ExprValue FormParser::record(std::optional<ast::ExprId> base, ast::RecordIdentity identity) {
    if (cursor_.take(TokenKind::symbol, U".")) {
        return record_access(std::move(base), std::move(identity));
    }
    auto fields = record_fields();
    return ast::RecordExpression{std::move(base), std::move(identity), std::move(fields)};
}

// Keep omitted fields omitted and preserve every explicit assignment, including wildcard fields.
std::vector<ast::RecordField> FormParser::record_fields() {
    expect(U"{");
    std::vector<ast::RecordField> fields;
    if (!cursor_.take_syntax(U"}")) {
        do {
            fields.push_back(record_field());
        } while (cursor_.take_syntax(U","));
        expect(U"}");
    }
    return fields;
}

// Field names accept atoms/variables, while values retain the permissive expr production.
ast::RecordField FormParser::record_field() {
    const auto begin = cursor_.offset();
    std::variant<ast::Atom, ast::Variable> name;
    if (!cursor_.empty() && cursor_.anchor().kind == TokenKind::variable) {
        name = ast::Variable{value<std::u32string>(*cursor_.consume())};
    } else {
        name = ast::Atom{value<std::u32string>(category(TokenKind::atom, "record field name"))};
    }
    expect(U"=");
    auto child = expression();
    return {std::move(name), std::move(child), builder_.source(begin, cursor_.offset(), begin)};
}
} // namespace erlang_aot

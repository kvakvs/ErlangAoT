#include "forms.hpp"

namespace erlang_aot {
namespace {
// Structural postfixes bind only to expr_max or their own recursive production family.
bool is_map(const ast::ExprValue &value) { return std::holds_alternative<ast::MapExpression>(value); }

bool is_record(const ast::ExprValue &value) {
    return std::holds_alternative<ast::RecordExpression>(value) || std::holds_alternative<ast::RecordAccess>(value) ||
           std::holds_alternative<ast::RecordIndex>(value);
}
} // namespace

// Dispatch hash syntax before general expression continuation, preserving pattern restrictions.
ast::ExprId FormParser::structural(OperatorContext context) {
    const auto begin = cursor_.offset();
    const bool hashed = syntax(cursor_.anchor(), U"#") || syntax(cursor_.anchor(), U"#_");
    auto value = hashed ? hash({}, context != OperatorContext::pattern) : primary(context);
    auto result = make(std::move(value), begin, begin);
    if (context != OperatorContext::pattern) {
        while (syntax(cursor_.anchor(), U"#") || syntax(cursor_.anchor(), U"#_")) {
            result = hash_suffix(std::move(result));
        }
    }
    return result;
}

// Keep postfix chains iterative and anchor each new node at its own hash token.
ast::ExprId FormParser::hash_suffix(ast::ExprId base) {
    const auto begin = builder_.view().expression(base).source.begin;
    const auto anchor = cursor_.offset();
    auto value = hash(std::move(base));
    return make(std::move(value), begin, anchor);
}

// Local record chains and map chains recur; qualified/inferred postfixes require expr_max.
void FormParser::check_hash_base(const ast::ExprId &base, bool map, bool local) const {
    const auto &value = builder_.view().expression(base).value;
    if (is_map(value)) {
        if (!map)
            fail(DiagnosticCode::parser_syntax, "record postfix requires a primary or record base");
    } else if (is_record(value)) {
        if (map || !local)
            fail(DiagnosticCode::parser_syntax, "postfix requires a primary expression base");
    }
}

// Discriminate maps and record identities without allowing arbitrary expr postfix bases.
ast::ExprValue FormParser::hash(std::optional<ast::ExprId> base, bool comprehension) {
    const auto *next = cursor_.peek(1);
    if (syntax(cursor_.anchor(), U"#") && next && syntax(*next, U"{")) {
        if (base)
            check_hash_base(*base, true, false);
        cursor_.consume();
        return map(std::move(base), comprehension);
    }
    const auto *name = cursor_.peek(1);
    const bool index_name = name && name->kind == TokenKind::atom;
    auto identity = record_identity();
    if (base)
        check_hash_base(*base, false, std::holds_alternative<ast::UnresolvedRecordName>(identity.value));
    if (!base && syntax(cursor_.anchor(), U".") && !index_name) {
        fail(DiagnosticCode::parser_syntax, "record index requires an atom name");
    }
    return record(std::move(base), std::move(identity));
}
} // namespace erlang_aot

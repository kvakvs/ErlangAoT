#include "term_value.hpp"

namespace erlang_aot {
void TermNormalizer::segment(Value &output, const ast::BinarySegment &value) const {
    std::vector<Token> modifiers;
    if (value.modifiers) {
        for (const auto &modifier : *value.modifiers) {
            Token token;
            token.kind = TokenKind::atom;
            token.value = modifier.name.name;
            modifiers.push_back(token);
            if (modifier.parameter) {
                token.kind = TokenKind::integer;
                token.value = *modifier.parameter;
                modifiers.push_back(token);
            }
        }
    }
    std::optional<Value> size;
    if (value.size) {
        size = read(*value.size, false);
    }
    auto id = value.value;
    while (const auto *group = std::get_if<ast::Group>(&module_.expression(id).value)) {
        id = group->expression;
    }
    const auto string = std::holds_alternative<ast::StringLiteral>(module_.expression(id).value);
    const auto before = output.bits.size();
    append_literal_bits(output, read(id, false), size, modifiers, string);
    literal_work(work_, output.bits.size() - before);
}

Value TermNormalizer::operator()(const ast::Bitstring &value) const {
    Value result;
    result.kind = ValueKind::bits;
    for (const auto &item : value.segments) {
        segment(result, item);
    }
    return result;
}
} // namespace erlang_aot

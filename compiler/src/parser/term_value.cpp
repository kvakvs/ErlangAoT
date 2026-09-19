#include "term_value.hpp"
#include "attribute_values.hpp"
#include "parsing/operator_info.hpp"
#include <charconv>

namespace erlang_aot {
namespace {
// Apply exact map replacement independently of literal traversal.
void map_entry(Value &result, const Value &key, Value mapped, std::size_t &work) {
    literal_work(work, result.elements.size() + 1);
    std::size_t position = 0;
    while (position < result.elements.size() && compare(result.elements[position], key, true) < 0) {
        position += 2;
    }
    if (position < result.elements.size() && compare(result.elements[position], key, true) == 0) {
        result.elements[position + 1] = std::move(mapped);
    } else {
        result.elements.insert(result.elements.begin() + static_cast<std::ptrdiff_t>(position), {key, mapped});
    }
}
} // namespace

Value TermNormalizer::read(const ast::ExprId &id, bool farity) const {
    literal_work(work_, 1);
    auto visitor = *this;
    visitor.farity_ = farity;
    return module_.visit(id, visitor);
}

Value TermNormalizer::child(const ast::ExprId &id) const { return read(id, farity_); }

Value TermNormalizer::operator()(const ast::Atom &value) const { return atom(value.name); }

Value TermNormalizer::operator()(const ast::IntegerLiteral &value) const {
    literal_work(work_, value.value.decimal.size());
    Token token;
    token.kind = TokenKind::integer;
    token.value = value.value;
    return erlang_aot::literal_value(token);
}

Value TermNormalizer::operator()(const ast::FloatLiteral &value) const { return floating(value.value); }

Value TermNormalizer::operator()(const ast::CharacterLiteral &value) const { return integer(value.value); }

Value TermNormalizer::operator()(const ast::StringLiteral &value) const {
    literal_work(work_, value.value.size());
    std::vector<Value> characters;
    for (const auto c : value.value) {
        characters.push_back(integer(c));
    }
    return list(std::move(characters));
}

Value TermNormalizer::operator()(const ast::Group &value) const { return child(value.expression); }

Value TermNormalizer::operator()(const ast::UnaryExpression &value) const {
    const auto spelling = operator_spelling(value.operation);
    const auto &operand = module_.expression(value.operand).value;
    if (const auto *group = std::get_if<ast::Group>(&operand)) {
        return (*this)(ast::UnaryExpression{value.operation, group->expression});
    }
    const bool scalar = std::holds_alternative<ast::IntegerLiteral>(operand) ||
                        std::holds_alternative<ast::FloatLiteral>(operand) ||
                        std::holds_alternative<ast::CharacterLiteral>(operand);
    if (!scalar || (spelling != U"+" && spelling != U"-")) {
        throw EvaluationFailure();
    }
    return evaluate_operator(spelling, {read(value.operand, false)});
}

Value TermNormalizer::operator()(const ast::BinaryExpression &value) const {
    if (!farity_ || operator_spelling(value.operation) != U"/") {
        throw EvaluationFailure();
    }
    if (!std::holds_alternative<ast::Atom>(ungroup(module_, value.left).value) ||
        !std::holds_alternative<ast::IntegerLiteral>(ungroup(module_, value.right).value)) {
        throw EvaluationFailure();
    }
    const auto name = read(value.left, false);
    const auto arity = read(value.right, false);
    Value result;
    result.kind = ValueKind::tuple;
    result.elements = {name, arity};
    return result;
}

Value TermNormalizer::operator()(const ast::Tuple &value) const {
    Value result;
    result.kind = ValueKind::tuple;
    for (const auto &id : value.elements) {
        result.elements.push_back(child(id));
    }
    return result;
}

Value TermNormalizer::operator()(const ast::List &value) const {
    std::vector<Value> values;
    values.reserve(value.elements.size());
    for (const auto &id : value.elements) {
        values.push_back(child(id));
    }
    auto result = list(std::move(values));
    if (value.tail) {
        result.tail = std::make_shared<Value>(child(*value.tail));
    }
    return result;
}

Value TermNormalizer::operator()(const ast::MapExpression &value) const {
    if (value.base) {
        throw EvaluationFailure();
    }
    Value result;
    result.kind = ValueKind::map;
    for (const auto &field : value.fields) {
        if (field.kind != ast::MapFieldKind::associate) {
            throw EvaluationFailure();
        }
        auto key = read(field.key, false);
        auto mapped = child(field.value);
        map_entry(result, key, std::move(mapped), work_);
    }
    return result;
}

Value TermNormalizer::operator()(const ast::RemoteFunReference &value) const {
    const auto *module = std::get_if<ast::Atom>(&value.module);
    const auto *name = std::get_if<ast::Atom>(&value.name);
    const auto *arity = std::get_if<Integer>(&value.arity);
    if (!module || !name || !arity) {
        throw EvaluationFailure();
    }
    unsigned count = 0;
    const auto *end = arity->decimal.data() + arity->decimal.size();
    const auto parsed = std::from_chars(arity->decimal.data(), end, count);
    if (parsed.ec != std::errc{} || parsed.ptr != end || count > 255) {
        throw EvaluationFailure();
    }
    Value result;
    result.kind = ValueKind::function;
    result.elements = {atom(module->name), atom(name->name), integer(count)};
    return result;
}
} // namespace erlang_aot

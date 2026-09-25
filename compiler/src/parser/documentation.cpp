#include "attribute_values.hpp"
#include "forms.hpp"
#include "term_value.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {
// Restrict documentation scalars to the forms recognized by build_attribute.
bool documentation_literal(const ast::Module &module, const ast::ExprId &id, const Value &value) {
    const auto &expression = ungroup(module, id).value;
    if (std::holds_alternative<ast::StringLiteral>(expression)) {
        return true;
    }
    if (value.kind == ValueKind::atom) {
        return value.text == U"true" || value.text == U"false" || value.text == U"hidden";
    }
    if (value.kind == ValueKind::bits) {
        return value.bits.size() % 8 == 0;
    }
    const auto *tuple = std::get_if<ast::Tuple>(&expression);
    if (!tuple || tuple->elements.size() != 2) {
        return false;
    }
    return attribute_as<ast::Atom>(module, tuple->elements[0]).name == U"file" &&
           std::holds_alternative<ast::StringLiteral>(ungroup(module, tuple->elements[1]).value);
}

// Sort exact metadata keys stably so the builder can retain their final values.
std::vector<std::pair<Value, ast::MapField>> metadata(const ast::Module &module, const ast::MapExpression &map,
                                                      std::size_t &work) {
    if (map.base) {
        throw EvaluationFailure();
    }
    std::vector<std::pair<Value, ast::MapField>> result;
    for (const auto &field : map.fields) {
        if (field.kind != ast::MapFieldKind::associate) {
            throw EvaluationFailure();
        }
        result.emplace_back(TermNormalizer(module, work).read(field.key, false), field);
    }
    std::stable_sort(result.begin(), result.end(), [&work](const auto &a, const auto &b) {
        literal_work(work, 1);
        return compare(a.first, b.first, true) < 0;
    });
    return result;
}
} // namespace

std::vector<ast::DocumentationEntry> FormParser::documentation_entries(const ast::MapExpression &map, bool module) {
    std::vector<ast::DocumentationEntry> entries;
    std::optional<Value> previous;
    for (const auto &[key, field] : metadata(builder_.view(), map, work_)) {
        const bool equiv = !module && key.kind == ValueKind::atom && key.text == U"equiv" &&
                           std::holds_alternative<ast::CallExpression>(ungroup(builder_.view(), field.value).value);
        auto value = equiv ? std::variant<ast::TermId, ast::ExprId>{field.value}
                           : std::variant<ast::TermId, ast::ExprId>{term(field.value)};
        if (previous && compare(*previous, key, true) == 0) {
            entries.pop_back();
        }
        entries.push_back({.key = term(field.key, false), .value = std::move(value)});
        previous = key;
    }
    return entries;
}

ast::DocumentationAttribute FormParser::documentation(const bool module, const ast::ExprId &id) {
    const auto &expression = ungroup(builder_.view(), id).value;
    if (const auto *map = std::get_if<ast::MapExpression>(&expression)) {
        return {.module = module, .value = documentation_entries(*map, module)};
    }
    const auto value = TermNormalizer(builder_.view(), work_).read(id, false);
    if (!documentation_literal(builder_.view(), id, value)) {
        throw EvaluationFailure();
    }
    return {.module = module, .value = term_value(value, builder_.view().expression(id).source)};
}
} // namespace erlang_aot

#include "forms.hpp"
#include "term_value.hpp"

namespace erlang_aot {
ast::TermId FormParser::term(const ast::ExprId &expression, const bool farity) {
    const auto source = builder_.view().expression(expression).source;
    return term_value(TermNormalizer(builder_.view(), work_).read(expression, farity), source);
}

ast::TermId FormParser::term_value(const Value &value, const ast::NodeSource &source) {
    enter();
    auto result = term_container(value, source);
    --depth_;
    node();
    return builder_.term(std::move(result), source);
}

ast::TermValue FormParser::term_container(const Value &value, const ast::NodeSource &source) {
    switch (value.kind) {
    case ValueKind::atom:
        return ast::Atom{value.text};
    case ValueKind::integer:
        return ast::IntegerLiteral{{value.integer.str()}};
    case ValueKind::floating:
        return ast::FloatLiteral{value.real};
    case ValueKind::bits:
        return ast::TermBits{value.bits};
    case ValueKind::function:
        return ast::TermFunction{.module = {value.elements[0].text},
                                 .name = {value.elements[1].text},
                                 .arity = {value.elements[2].integer.str()}};
    case ValueKind::map: {
        ast::TermMap result;
        for (std::size_t i = 0; i < value.elements.size(); i += 2) {
            result.entries.emplace_back(term_value(value.elements[i], source),
                                        term_value(value.elements[i + 1], source));
        }
        return result;
    }
    default:
        break;
    }
    return term_sequence(value, source);
}

ast::TermValue FormParser::term_sequence(const Value &value, const ast::NodeSource &source) {
    std::vector<ast::TermId> elements;
    elements.reserve(value.elements.size());
    for (const auto &item : value.elements) {
        elements.push_back(term_value(item, source));
    }
    if (value.kind == ValueKind::tuple) {
        return ast::TermTuple{std::move(elements)};
    }
    std::optional<ast::TermId> tail;
    if (value.tail) {
        tail = term_value(*value.tail, source);
    }
    return ast::TermList{.elements = std::move(elements), .tail = std::move(tail)};
}
} // namespace erlang_aot

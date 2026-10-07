#include "capabilities.hpp"

namespace erlang_aot::semantic {
namespace {
// The input expression of every generator kind; filters have none.
struct GeneratorInput {
    std::optional<ast::ExprId> operator()(const ast::FilterQualifier &) const { return {}; }

    template <typename Generator> std::optional<ast::ExprId> operator()(const Generator &value) const {
        return value.input;
    }
};

// Patterns a generator matches each element with, in match order.
struct GeneratorPatterns {
    std::vector<ast::PatternSyntaxId> operator()(const ast::FilterQualifier &) const { return {}; }

    std::vector<ast::PatternSyntaxId> operator()(const ast::MapGenerator &value) const {
        return {value.key, value.value};
    }

    template <typename Generator> std::vector<ast::PatternSyntaxId> operator()(const Generator &value) const {
        return {value.pattern};
    }
};

// The strict arrow of every generator kind; filters are not strict.
struct GeneratorStrictness {
    bool operator()(const ast::FilterQualifier &) const { return false; }

    template <typename Generator> bool operator()(const Generator &value) const { return value.strict; }
};
} // namespace

const std::vector<ast::ComprehensionQualifier> *comprehension_qualifiers(const ast::ExprValue &value) {
    if (const auto *list = std::get_if<ast::ListComprehension>(&value)) {
        return &list->qualifiers;
    }
    if (const auto *map = std::get_if<ast::MapComprehension>(&value)) {
        return &map->qualifiers;
    }
    if (const auto *binary = std::get_if<ast::BinaryComprehension>(&value)) {
        return &binary->qualifiers;
    }
    return nullptr;
}

std::span<const ast::Qualifier> zipped(const ast::ComprehensionQualifier &qualifier) {
    if (const auto *zip = std::get_if<ast::ZippedQualifier>(&qualifier)) {
        return zip->qualifiers;
    }
    return {&std::get<ast::Qualifier>(qualifier), 1};
}

std::optional<ast::ExprId> generator_input(const ast::Qualifier &qualifier) {
    return std::visit(GeneratorInput{}, qualifier.value);
}

std::vector<ast::PatternSyntaxId> generator_patterns(const ast::Qualifier &qualifier) {
    return std::visit(GeneratorPatterns{}, qualifier.value);
}

bool strict_generator(const ast::Qualifier &qualifier) { return std::visit(GeneratorStrictness{}, qualifier.value); }

std::vector<ast::ExprId> comprehension_templates(const ast::ExprValue &value) {
    if (const auto *list = std::get_if<ast::ListComprehension>(&value)) {
        return list->templates;
    }
    if (const auto *binary = std::get_if<ast::BinaryComprehension>(&value)) {
        return {binary->expression};
    }
    // OTP evaluates the first field's value before its key, every later field's key first.
    const auto &fields = std::get<ast::MapComprehension>(value).templates;
    std::vector<ast::ExprId> result;
    result.reserve(2 * fields.size());
    for (const auto &field : fields) {
        const bool first = result.empty();
        result.push_back(first ? field.value : field.key);
        result.push_back(first ? field.key : field.value);
    }
    return result;
}

std::vector<ast::ExprId> comprehension_children(const ast::ExprValue &value) {
    std::vector<ast::ExprId> result;
    for (const auto &qualifier : *comprehension_qualifiers(value)) {
        for (const auto &simple : zipped(qualifier)) {
            const auto *filter = std::get_if<ast::FilterQualifier>(&simple.value);
            result.push_back(filter ? filter->expression : *generator_input(simple));
        }
    }
    const auto templates = comprehension_templates(value);
    result.insert(result.end(), templates.begin(), templates.end());
    return result;
}
} // namespace erlang_aot::semantic

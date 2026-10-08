#include "capabilities.hpp"

namespace clause::semantic {
namespace {
// Embedded sizes retain their analyzed preceding-segment scope without revisiting segment definitions.
void binary_reads(const Module &module, const NormalizedPattern &pattern, std::vector<ast::ExprId> &result) {
    const auto &binary = std::get<ast::Bitstring>(module.syntax->expression(pattern.expression).value);
    for (const auto &segment : binary.segments) {
        if (segment.size) {
            result.push_back(*segment.size);
        }
    }
}

// Map keys are expression reads in the incoming pattern scope.
void map_reads(const Module &module, const NormalizedPattern &pattern, std::vector<ast::ExprId> &result) {
    const auto &map = std::get<ast::MapExpression>(module.syntax->expression(pattern.expression).value);
    for (const auto &field : map.fields) {
        result.push_back(field.key);
    }
}
} // namespace

std::vector<ast::ExprId> pattern_reads(const Module &module, const Function &function) {
    std::vector<ast::ExprId> result;
    for (const auto &pattern : function.patterns) {
        if (pattern.kind == PatternKind::bitstring) {
            binary_reads(module, pattern, result);
        }
        if (pattern.kind == PatternKind::map) {
            map_reads(module, pattern, result);
        }
    }
    return result;
}
} // namespace clause::semantic

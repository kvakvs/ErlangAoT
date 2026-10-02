#include "capabilities.hpp"

namespace erlang_aot::semantic {
std::vector<ast::ExprId> pattern_reads(const Module &module, const Function &function) {
    std::vector<ast::ExprId> result;
    for (const auto &pattern : function.patterns) {
        if (pattern.kind != PatternKind::map) {
            continue;
        }
        const auto &map = std::get<ast::MapExpression>(module.syntax->expression(pattern.expression).value);
        for (const auto &field : map.fields) {
            result.push_back(field.key);
        }
    }
    return result;
}
} // namespace erlang_aot::semantic

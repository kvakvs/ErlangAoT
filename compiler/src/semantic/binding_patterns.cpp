#include "binding_state.hpp"

namespace erlang_aot::semantic {
namespace {
// Ordinary children retain source evaluation order; branch/closure scopes are deliberately opaque here.
struct Children {
    template <typename T> std::vector<ast::ExprId> operator()(const T &) const { return {}; }

    std::vector<ast::ExprId> operator()(const ast::Group &v) const { return {v.expression}; }

    std::vector<ast::ExprId> operator()(const ast::UnaryExpression &v) const { return {v.operand}; }

    std::vector<ast::ExprId> operator()(const ast::BinaryExpression &v) const { return {v.left, v.right}; }

    std::vector<ast::ExprId> operator()(const ast::MatchExpression &v) const { return {v.left, v.right}; }

    std::vector<ast::ExprId> operator()(const ast::CatchExpression &v) const { return {v.expression}; }

    std::vector<ast::ExprId> operator()(const ast::Tuple &v) const { return v.elements; }

    std::vector<ast::ExprId> operator()(const ast::BlockExpression &v) const { return v.body; }

    std::vector<ast::ExprId> operator()(const ast::RemoteExpression &v) const { return {v.module, v.function}; }

    std::vector<ast::ExprId> operator()(const ast::RecordAccess &v) const { return {v.base}; }

    std::vector<ast::ExprId> operator()(const ast::List &v) const {
        auto result = v.elements;
        if (v.tail) {
            result.push_back(*v.tail);
        }
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::CallExpression &v) const {
        std::vector<ast::ExprId> result{v.target};
        result.insert(result.end(), v.arguments.begin(), v.arguments.end());
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::MapExpression &v) const {
        std::vector<ast::ExprId> result;
        if (v.base) {
            result.push_back(*v.base);
        }
        for (const auto &field : v.fields) {
            result.push_back(field.key);
            result.push_back(field.value);
        }
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::RecordExpression &v) const {
        std::vector<ast::ExprId> result;
        if (v.base) {
            result.push_back(*v.base);
        }
        for (const auto &field : v.fields) {
            result.push_back(field.value);
        }
        return result;
    }

    std::vector<ast::ExprId> operator()(const ast::Bitstring &v) const {
        std::vector<ast::ExprId> result;
        for (const auto &segment : v.segments) {
            result.push_back(segment.value);
            if (segment.size) {
                result.push_back(*segment.size);
            }
        }
        return result;
    }
};

} // namespace

std::vector<ast::ExprId> binding_children(const ast::ExprValue &value) { return std::visit(Children{}, value); }
} // namespace erlang_aot::semantic

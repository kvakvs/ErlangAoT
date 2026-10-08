#include "children.hpp"

namespace clause::ast {
void Builder::validate(const ExprValue &value) const {
    if (!active_) {
        throw std::logic_error("AST children require a form transaction");
    }
    std::visit(Children{.builder = *this, .form = *active_}, value);
}
} // namespace clause::ast

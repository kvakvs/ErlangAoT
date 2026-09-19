#include "children.hpp"

namespace erlang_aot::ast {
void Builder::validate(const ExprValue &value) const {
    if (!active_) {
        throw std::logic_error("AST children require a form transaction");
    }
    std::visit(Children{*this, *active_}, value);
}
} // namespace erlang_aot::ast

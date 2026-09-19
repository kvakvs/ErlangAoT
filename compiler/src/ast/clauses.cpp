#include "children.hpp"
#include "storage.hpp"

namespace erlang_aot::ast {
// Store a category-safe pattern wrapper only after validating its expression owner.
PatternSyntaxId Builder::pattern(PatternValue value, NodeSource source) {
    validate(source);
    const auto &child = std::visit([](const auto &pattern) -> const ExprId & { return pattern.expression; }, value);
    validate(module_.expression(child).source);
    return module_.storage_->patterns.append({std::move(value), std::move(source)});
}

// Functions and fun expressions use the same owner, pattern and arity checks.
void Builder::validate(const FormValue &value) const {
    if (!active_) {
        throw std::logic_error("function requires an active transaction");
    }
    std::visit(Children{*this, *active_}, value);
}
} // namespace erlang_aot::ast

#include "tree.hpp"

namespace erlang_aot::printing {
namespace {
struct Component {
    // Compact reference components still distinguish atoms, variables and integers.
    std::string operator()(const ast::Atom &value) const { return "atom:" + atom(value); }

    std::string operator()(const ast::Variable &value) const { return "variable:" + utf8(value.name); }

    std::string operator()(const Integer &value) const { return value.decimal; }
};
} // namespace

void TreePrinter::operator()(const ast::LocalFunReference &value) const {
    output_ << "LocalFunReference name=" << atom(value.name) << " arity=" << value.arity.decimal;
}

void TreePrinter::operator()(const ast::RemoteFunReference &value) const {
    output_ << "RemoteFunReference module=" << std::visit(Component{}, value.module)
            << " name=" << std::visit(Component{}, value.name) << " arity=" << std::visit(Component{}, value.arity);
}

void TreePrinter::operator()(const ast::FunExpression &value) {
    output_ << "FunExpression name=" << (value.name ? utf8(value.name->name) : "<anonymous>")
            << " clauses=" << value.clauses.size();
    objects("clause", value.clauses);
}

void TreePrinter::operator()(const ast::TryExpression &value) {
    output_ << "TryExpression body=" << value.body.size() << " of=" << (value.of ? "present" : "none")
            << " catch=" << (value.handlers ? "present" : "none") << " after=" << (value.after ? "present" : "none");
    handles("body", value.body);
    if (value.of)
        objects("of", *value.of);
    if (value.handlers)
        objects("catch", *value.handlers);
    if (value.after)
        handles("after", *value.after);
}

void TreePrinter::operator()(const ast::CatchClause &value) {
    output_ << "CatchClause class="
            << (value.exception_class ? std::visit(Component{}, *value.exception_class) : "<omitted>")
            << " stacktrace=" << (value.stacktrace ? utf8(value.stacktrace->name) : "<omitted>");
    child("reason", value.reason);
    if (value.guard)
        child("guard", &*value.guard);
    handles("body", value.body);
}

void TreePrinter::maybe_child(std::string role, const ast::ExprId &value) { child(std::move(role), value); }

void TreePrinter::maybe_child(std::string role, const ast::MaybeMatch &value) { child(std::move(role), &value); }

void TreePrinter::operator()(const ast::MaybeExpression &value) {
    output_ << "MaybeExpression body=" << value.body.size() << " else=" << (value.otherwise ? "present" : "none");
    std::size_t index = 0;
    for (const auto &item : value.body) {
        const auto role = "body[" + std::to_string(index++) + ']';
        std::visit([&](const auto &part) { maybe_child(role, part); }, item);
    }
    if (value.otherwise)
        objects("else", *value.otherwise);
}

void TreePrinter::operator()(const ast::MaybeMatch &value) {
    output_ << "MaybeMatch";
    child("pattern", value.pattern);
    child("value", value.value);
}
} // namespace erlang_aot::printing

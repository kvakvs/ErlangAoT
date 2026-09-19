#include "tree.hpp"

namespace erlang_aot::printing {
void TreePrinter::operator()(const ast::BlockExpression &value) {
    output_ << "BlockExpression body=" << value.body.size();
    handles("body", value.body);
}

void TreePrinter::operator()(const ast::CaseExpression &value) {
    output_ << "CaseExpression clauses=" << value.clauses.size();
    child("value", value.value);
    objects("clause", value.clauses);
}

void TreePrinter::operator()(const ast::IfExpression &value) {
    output_ << "IfExpression clauses=" << value.clauses.size();
    objects("clause", value.clauses);
}

void TreePrinter::operator()(const ast::ReceiveExpression &value) {
    output_ << "ReceiveExpression clauses=" << value.clauses.size() << " after=" << (value.after ? "present" : "none");
    objects("clause", value.clauses);
    if (value.after) {
        child("after", &*value.after);
    }
}

void TreePrinter::operator()(const ast::BranchClause &value) {
    output_ << "BranchClause guard=" << (value.guard ? "present" : "none") << " body=" << value.body.size();
    child("pattern", value.pattern);
    if (value.guard) {
        child("guard", &*value.guard);
    }
    handles("body", value.body);
}

void TreePrinter::operator()(const ast::IfClause &value) {
    output_ << "IfClause body=" << value.body.size();
    child("guard", &value.guard);
    handles("body", value.body);
}

void TreePrinter::operator()(const ast::ReceiveTimeout &value) {
    output_ << "ReceiveTimeout body=" << value.body.size();
    child("timeout", value.timeout);
    handles("body", value.body);
}
} // namespace erlang_aot::printing

#include "tree.hpp"

namespace erlang_aot::printing {
void TreePrinter::operator()(const ast::ModuleAttribute &value) {
    output_ << "ModuleAttribute name=" << atom(value.name);
}

void TreePrinter::operator()(const ast::FileAttribute &value) {
    output_ << "FileAttribute name=" << literal(TokenKind::string, value.name) << " line=" << value.line.decimal;
}

void TreePrinter::operator()(const ast::Function &value) {
    output_ << "Function name=" << atom(value.name) << " arity=" << value.clauses.front().arguments.size()
            << " clauses=" << value.clauses.size();
    objects("clause", value.clauses);
}

void TreePrinter::operator()(const ast::FunctionClause &value) {
    output_ << "FunctionClause arguments=" << value.arguments.size() << " guard=" << (value.guard ? "present" : "none")
            << " body=" << value.body.size();
    handles("argument", value.arguments);
    if (value.guard) {
        child("guard", &*value.guard);
    }
    handles("body", value.body);
}

void TreePrinter::operator()(const ast::GuardSyntax &value) {
    output_ << "GuardSyntax alternatives=" << value.alternatives.size();
    objects("alternative", value.alternatives);
}

void TreePrinter::operator()(const ast::GuardConjunction &value) {
    output_ << "GuardConjunction tests=" << value.tests.size();
    handles("test", value.tests);
}

void TreePrinter::operator()(const ast::RestrictedPattern &value) {
    output_ << "RestrictedPattern";
    child("expression", value.expression);
}

void TreePrinter::operator()(const ast::PatternCandidate &value) {
    output_ << "PatternCandidate";
    child("expression", value.expression);
}
} // namespace erlang_aot::printing

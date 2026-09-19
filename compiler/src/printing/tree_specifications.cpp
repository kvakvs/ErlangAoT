#include "tree.hpp"

namespace erlang_aot::printing {
void TreePrinter::operator()(const ast::Specification &value) {
    output_ << (value.callback ? "Callback" : "Specification") << " name=" << atom(value.name)
            << " arity=" << value.arity;
    if (value.module) {
        output_ << " module=" << atom(*value.module);
    }
    objects("signature", value.signatures);
}

void TreePrinter::operator()(const ast::SpecificationSignature &value) {
    output_ << "SpecificationSignature constraints=" << value.constraints.size();
    child("function", &value.function);
    objects("constraint", value.constraints);
}

void TreePrinter::operator()(const ast::TypeConstraint &value) {
    output_ << "TypeConstraint variable=" << utf8(value.variable.name) << " legacy=" << value.legacy;
    child("bound", value.bound);
}
} // namespace erlang_aot::printing

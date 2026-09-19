#include "children.hpp"

namespace erlang_aot::ast {
void Children::signature(const SpecificationSignature &value) const {
    source(value.source);
    if (!value.function.result)
        throw std::invalid_argument("specification requires result type");
    (*this)(value.function);
    for (const auto &constraint : value.constraints) {
        source(constraint.source);
        if (constraint.variable.name == U"_")
            throw std::invalid_argument("wildcard constraint variable");
        child(constraint.bound);
    }
}

void Children::operator()(const Specification &value) const {
    if (value.signatures.empty())
        throw std::invalid_argument("specification requires signatures");
    const auto &arguments = value.signatures.front().function.arguments;
    if (!arguments || arguments->size() != value.arity)
        throw std::invalid_argument("invalid first specification arity");
    for (const auto &overload : value.signatures)
        signature(overload);
}
} // namespace erlang_aot::ast

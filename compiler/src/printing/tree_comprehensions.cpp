#include "tree.hpp"

namespace erlang_aot::printing {
void TreePrinter::qualifier_child(std::string role, const ast::Qualifier &value) { child(std::move(role), &value); }

void TreePrinter::qualifier_child(std::string role, const ast::ZippedQualifier &value) {
    child(std::move(role), &value);
}

void TreePrinter::qualifier_children(const std::vector<ast::ComprehensionQualifier> &values) {
    std::size_t index = 0;
    for (const auto &item : values) {
        const auto role = "qualifier[" + std::to_string(index++) + ']';
        std::visit([&](const auto &value) { qualifier_child(role, value); }, item);
    }
}

void TreePrinter::operator()(const ast::ListComprehension &value) {
    output_ << "ListComprehension templates=" << value.templates.size() << " qualifiers=" << value.qualifiers.size();
    handles("template", value.templates);
    qualifier_children(value.qualifiers);
}

void TreePrinter::operator()(const ast::MapComprehension &value) {
    output_ << "MapComprehension templates=" << value.templates.size() << " qualifiers=" << value.qualifiers.size();
    objects("template", value.templates);
    qualifier_children(value.qualifiers);
}

void TreePrinter::operator()(const ast::BinaryComprehension &value) {
    output_ << "BinaryComprehension qualifiers=" << value.qualifiers.size();
    child("template", value.expression);
    qualifier_children(value.qualifiers);
}

void TreePrinter::operator()(const ast::Qualifier &value) { std::visit(*this, value.value); }

void TreePrinter::operator()(const ast::ZippedQualifier &value) {
    output_ << "ZippedQualifier qualifiers=" << value.qualifiers.size();
    objects("qualifier", value.qualifiers);
}

void TreePrinter::operator()(const ast::FilterQualifier &value) {
    output_ << "FilterQualifier";
    child("expression", value.expression);
}

void TreePrinter::operator()(const ast::ListGenerator &value) {
    output_ << "ListGenerator operator=" << (value.strict ? "<:-" : "<-");
    child("pattern", value.pattern);
    child("input", value.input);
}

void TreePrinter::operator()(const ast::BinaryGenerator &value) {
    output_ << "BinaryGenerator operator=" << (value.strict ? "<:=" : "<=");
    child("pattern", value.pattern);
    child("input", value.input);
}

void TreePrinter::operator()(const ast::MapGenerator &value) {
    output_ << "MapGenerator operator=" << (value.strict ? "<:-" : "<-");
    child("key", value.key);
    child("value", value.value);
    child("input", value.input);
}
} // namespace erlang_aot::printing

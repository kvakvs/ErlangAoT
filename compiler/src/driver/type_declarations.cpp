#include "type_declarations.hpp"
#include "display.hpp"
#include "type_format.hpp"
#include <array>
#include <ostream>

namespace erlang_aot::cli {
namespace {
namespace types = semantic::types;

// Preserve formal parameter names in source order, independently of internal variable scope identities.
std::string parameters(const std::vector<std::string> &names) {
    std::string text;
    for (const auto &name : names) {
        if (!text.empty()) {
            text += ", ";
        }
        text += quote_text(name);
    }
    return '[' + text + ']';
}

// Keep unresolved metadata visibly unknown instead of inventing a declaration body.
std::string body(const types::Registry &registry, const types::Declaration &declaration) {
    if (!declaration.body) {
        return "term() [unknown declaration]";
    }
    return type_text(registry.graph, *declaration.body);
}

// Print alias/opaque/nominal identity and visibility without recursively opening referenced declarations.
void aliases(std::ostream &output, const types::Registry &registry, const semantic::Module &module) {
    constexpr std::array names{"type", "opaque", "nominal"};
    for (const auto &declaration : registry.declarations) {
        if (declaration.module != &module) {
            continue;
        }
        output << "  declared " << names.at(static_cast<std::size_t>(declaration.kind)) << ' '
               << quote_text(declaration.key.name) << '/' << declaration.key.arity
               << " exported=" << (declaration.exported ? "true" : "false")
               << " parameters=" << parameters(declaration.parameters) << " = " << body(registry, declaration) << '\n';
    }
}

// Keep each source overload and its constraints separate in declaration order.
void overloads(std::ostream &output, const types::Registry &registry, const types::Contract &contract) {
    for (const auto &overload : contract.overloads) {
        output << "    overload " << type_text(registry.graph, overload.function);
        for (const auto &[variable, bound] : overload.constraints) {
            output << " when " << quote_text(variable) << " :: " << type_text(registry.graph, bound);
        }
        output << '\n';
    }
}

// Distinguish specifications from callbacks, including optional callback declarations.
void contracts(std::ostream &output, const types::Registry &registry, const semantic::Module &module) {
    for (const auto &contract : registry.contracts) {
        if (contract.module != &module) {
            continue;
        }
        output << "  declared " << (contract.callback ? "callback " : "spec ") << quote_text(contract.key.name) << '/'
               << contract.key.arity << " optional=" << (contract.optional ? "true" : "false") << '\n';
        overloads(output, registry, contract);
    }
}

// Preserve record-field types as metadata even though constructing records is outside the executable subset.
void records(std::ostream &output, const types::Registry &registry, const semantic::Module &module) {
    for (const auto &[key, record] : registry.records) {
        if (record.module != &module) {
            continue;
        }
        output << "  declared record " << quote_text(key.second) << '\n';
        for (const auto &[name, type] : record.fields) {
            output << "    field " << quote_text(name) << " :: " << type_text(registry.graph, type) << '\n';
        }
    }
}
} // namespace

void print_declared_types(std::ostream &output, const semantic::types::Registry &registry,
                          const semantic::Module &module) {
    aliases(output, registry, module);
    contracts(output, registry, module);
    records(output, registry, module);
}
} // namespace erlang_aot::cli

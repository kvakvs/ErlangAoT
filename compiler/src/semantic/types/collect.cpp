#include "resolver.hpp"
#include <algorithm>
#include <set>

namespace erlang_aot::semantic::types {
namespace {
// Preserve all formal positions; OTP substitution uses the last occurrence of a repeated name.
std::vector<std::string> parameters(const ast::TypeDeclaration &decl) {
    std::vector<std::string> names;
    for (const auto &parameter : decl.parameters) {
        const auto name = utf8(parameter.name);
        names.push_back(name);
    }
    return names;
}

// Index all type identities before inspecting bodies so forward and recursive references work.
void declaration(Registry &registry, const Module &module, const ast::FormId &id, const ast::TypeDeclaration &decl,
                 const Reporter &out) {
    const auto &source = module.syntax->form(id).source;
    Key key{utf8(module.name), utf8(decl.name.name), decl.parameters.size()};
    if (key.arity > 255) {
        report(module, &source, "type arity exceeds 255", out);
        return;
    }
    if (!registry.lookup.emplace(key, registry.declarations.size()).second) {
        report(module, &source, "duplicate type declaration " + key.name, out);
        return;
    }
    registry.declarations.push_back({&module, id, std::move(key), decl.kind, parameters(decl), false, {}});
}

// Records are metadata only; expression defaults remain immutable, unevaluated syntax.
void record_declaration(Registry &registry, const Module &module, const ast::FormId &id,
                        const ast::RecordDeclaration &decl, const Reporter &out) {
    const auto &source = module.syntax->form(id).source;
    if (!registry.records.emplace(std::pair{utf8(module.name), utf8(decl.name.name)}, Record{&module, id, {}}).second) {
        report(module, &source, "duplicate record declaration", out);
    }
    std::set<std::u32string> fields;
    for (const auto &field : decl.fields) {
        if (!fields.insert(field.name.name).second) {
            report(module, &field.source, "duplicate record field", out);
        }
    }
}

// Each attribute item must be exactly a literal {Name,Arity} pair.
std::optional<FunctionKey> name_arity(const Module &module, const ast::TermId &id, const Reporter &out) {
    const auto &term = module.syntax->term(id);
    const auto *tuple = std::get_if<ast::TermTuple>(&term.value);
    if (!tuple || tuple->elements.size() != 2) {
        report(module, &term.source, "expected name/arity metadata", out);
        return {};
    }
    const auto *name = std::get_if<ast::Atom>(&module.syntax->term(tuple->elements[0]).value);
    const auto *number = std::get_if<ast::IntegerLiteral>(&module.syntax->term(tuple->elements[1]).value);
    const auto count = number ? arity(number->value) : std::nullopt;
    if (!name || !count) {
        report(module, &term.source, "invalid metadata name/arity", out);
        return {};
    }
    return FunctionKey{name->name, *count};
}

// Apply export visibility only after every local type declaration has been indexed.
void export_type(Registry &registry, const Module &module, const FunctionKey &key, const ast::NodeSource &source,
                 const Reporter &out) {
    const auto found = registry.lookup.find({utf8(module.name), utf8(key.name), key.arity});
    if (found == registry.lookup.end()) {
        report(module, &source, "export of undefined type " + utf8(key.name), out);
        return;
    }
    auto &decl = registry.declarations[found->second];
    if (decl.exported) {
        report(module, &source, "duplicate type export " + utf8(key.name), out);
    }
    decl.exported = true;
}

// Optional callbacks refer to declarations in the same module, never implementation functions.
void optional_callback(Registry &registry, const Module &module, const FunctionKey &key, const ast::NodeSource &source,
                       const Reporter &out) {
    const Key identity{utf8(module.name), utf8(key.name), key.arity};
    const auto found =
        std::ranges::find_if(registry.contracts, [&](const auto &c) { return c.callback && c.key == identity; });
    if (found == registry.contracts.end()) {
        report(module, &source, "optional callback is undefined: " + utf8(key.name), out);
        return;
    }
    if (found->optional) {
        report(module, &source, "duplicate optional callback", out);
    }
    found->optional = true;
}

// Generic literal metadata remains strictly shaped even though parser acceptance is broader.
void metadata(Registry &registry, const Module &module, const ast::GenericAttribute &attribute,
              const ast::NodeSource &source, const Reporter &out) {
    if (attribute.name.name != U"export_type" && attribute.name.name != U"optional_callbacks") {
        return;
    }
    const auto *list = std::get_if<ast::TermList>(&module.syntax->term(attribute.value).value);
    if (!list || list->tail) {
        report(module, &source, "expected proper metadata list", out);
        return;
    }
    const auto apply = attribute.name.name == U"export_type" ? export_type : optional_callback;
    for (const auto &item : list->elements) {
        if (const auto key = name_arity(module, item, out)) {
            apply(registry, module, *key, source, out);
        }
    }
}

// Specs and callbacks have separate namespaces and retain distinct source contracts.
void contract(Registry &registry, const Module &module, const ast::FormId &id, const ast::Specification &spec,
              const Reporter &out) {
    const auto &source = module.syntax->form(id).source;
    Key key{utf8(module.name), utf8(spec.name.name), spec.arity};
    if (key.arity > 255) {
        report(module, &source, "specification arity exceeds 255", out);
        return;
    }
    if (spec.module && spec.module->name != module.name) {
        report(module, &source, "specification module differs from current module", out);
    }
    if (!spec.callback && !module.lookup.contains({spec.name.name, spec.arity})) {
        report(module, &source, "specification for undefined function " + key.name, out);
    }
    const auto duplicate = std::ranges::any_of(
        registry.contracts, [&](const auto &c) { return c.key == key && c.callback == spec.callback; });
    if (duplicate) {
        report(module, &source, "duplicate specification " + key.name, out);
        return;
    }
    registry.contracts.push_back({&module, id, std::move(key), spec.callback, false, {}});
}

// Separate declaration collection from the literal metadata pass.
void forms(Registry &registry, const Module &module, const Reporter &out) {
    registry.modules.emplace(utf8(module.name), &module);
    for (const auto &id : module.syntax->forms()) {
        const auto &form = module.syntax->form(id);
        if (const auto *value = std::get_if<ast::TypeDeclaration>(&form.value)) {
            declaration(registry, module, id, *value, out);
        }
        if (const auto *value = std::get_if<ast::Specification>(&form.value)) {
            contract(registry, module, id, *value, out);
        }
        if (const auto *value = std::get_if<ast::RecordDeclaration>(&form.value)) {
            record_declaration(registry, module, id, *value, out);
        }
    }
}
} // namespace

void collect(Registry &registry, std::span<const std::unique_ptr<Module>> modules, const Reporter &out) {
    for (const auto &module : modules) {
        forms(registry, *module, out);
    }
    for (const auto &module : modules) {
        for (const auto &id : module->syntax->forms()) {
            const auto &form = module->syntax->form(id);
            if (const auto *value = std::get_if<ast::GenericAttribute>(&form.value)) {
                metadata(registry, *module, *value, form.source, out);
            }
        }
    }
}
} // namespace erlang_aot::semantic::types

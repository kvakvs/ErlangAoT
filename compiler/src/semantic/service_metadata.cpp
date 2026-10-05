#include "services.hpp"
#include <algorithm>

namespace erlang_aot::semantic {
namespace {
// no_auto_import selectors contain literal names/arities; dynamic or malformed metadata stays unavailable.
bool selector(const ast::Module &syntax, const ast::TermId &id) {
    const auto *tuple = std::get_if<ast::TermTuple>(&syntax.term(id).value);
    if (!tuple || tuple->elements.size() != 2) {
        return false;
    }
    const auto *name = std::get_if<ast::Atom>(&syntax.term(tuple->elements[0]).value);
    const auto *number = std::get_if<ast::IntegerLiteral>(&syntax.term(tuple->elements[1]).value);
    return name && number && arity(number->value).has_value();
}

// Auto-import suppression and warning-only nowarn_* options are inert; parse transforms and other compile
// behavior remain gated.
bool option(const ast::Module &syntax, const ast::TermValue &value) {
    if (const auto *name = std::get_if<ast::Atom>(&value)) {
        return name->name == U"no_auto_import" || name->name.starts_with(U"nowarn_");
    }
    const auto *tuple = std::get_if<ast::TermTuple>(&value);
    if (!tuple || tuple->elements.size() != 2) {
        return false;
    }
    const auto *name = std::get_if<ast::Atom>(&syntax.term(tuple->elements[0]).value);
    const auto *list = std::get_if<ast::TermList>(&syntax.term(tuple->elements[1]).value);
    if (!name || name->name != U"no_auto_import" || !list || list->tail) {
        return false;
    }
    return std::ranges::all_of(list->elements, [&](const auto &id) { return selector(syntax, id); });
}

// A bounded iterative list walk admits combinations of the same supported suppression metadata.
bool compile(const ast::Module &syntax, const ast::TermId &root) {
    std::vector<ast::TermId> pending{root};
    std::size_t work = 0;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (++work > 100'000) {
            return false;
        }
        const auto &value = syntax.term(id).value;
        if (const auto *list = std::get_if<ast::TermList>(&value)) {
            if (list->tail) {
                return false;
            }
            pending.insert(pending.end(), list->elements.begin(), list->elements.end());
        } else if (!option(syntax, value)) {
            return false;
        }
    }
    return true;
}
} // namespace

bool service_metadata(const ast::Module &syntax, const ast::FormValue &value) {
    if (const auto *attribute = std::get_if<ast::GenericAttribute>(&value)) {
        return attribute->name.name == U"compile" && compile(syntax, attribute->value);
    }
    const auto *attribute = std::get_if<ast::ImportAttribute>(&value);
    return attribute && attribute->module.name == U"erlang" &&
           std::ranges::all_of(attribute->functions, [](const auto &entry) {
               const auto count = arity(entry.arity);
               return count && guard_signature({entry.name.name, *count});
           });
}
} // namespace erlang_aot::semantic

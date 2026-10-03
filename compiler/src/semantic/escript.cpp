#include "escript.hpp"
#include <algorithm>
#include <array>

namespace erlang_aot::semantic {
namespace {
// OTP escript accepts these modes; ErlangAoT always compiles, so the value is only validated.
bool valid_mode(const ast::Module &syntax, const ast::GenericAttribute &attribute) {
    constexpr std::array<std::u32string_view, 4> modes{U"compile", U"interpret", U"debug", U"native"};
    const auto *atom = std::get_if<ast::Atom>(&syntax.term(attribute.value).value);
    return atom && std::ranges::contains(modes, atom->name);
}

// Report every -mode attribute whose value escript would reject.
void check_modes(const Module &module, const Reporter &out) {
    const auto &syntax = *module.syntax;
    for (const auto &id : syntax.forms()) {
        const auto &form = syntax.form(id);
        const auto *attribute = std::get_if<ast::GenericAttribute>(&form.value);
        if (attribute && attribute->name.name == U"mode" && !valid_mode(syntax, *attribute)) {
            report(module, &form.source, "illegal escript mode attribute; expected compile, interpret, debug or native",
                   out);
        }
    }
}

// Point at the module declaration, which escript headers may have synthesized on line 1.
const ast::NodeSource *declaration(const Module &module) {
    return module.declaration ? &module.syntax->form(*module.declaration).source : nullptr;
}
} // namespace

void index_escript(Module &module, const Reporter &out) {
    module.escript = true;
    check_modes(module, out);
    const auto found = module.lookup.find({U"main", 1});
    if (found == module.lookup.end()) {
        report(module, declaration(module), "escript does not define main/1", out);
        return;
    }
    module.functions[found->second].exported = true;
}
} // namespace erlang_aot::semantic

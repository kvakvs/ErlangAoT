#include "predefined.hpp"
#include "../semantic/behaviours.hpp"
#include <clause/compiler/lexer.hpp>

namespace clause::cli {
namespace {
// The -module form, whose location the generated function takes (OTP gives it no line).
const ast::Form *module_form(const ast::Module &syntax) {
    for (const auto &id : syntax.forms()) {
        if (std::holds_alternative<ast::ModuleAttribute>(syntax.form(id).value)) {
            return &syntax.form(id);
        }
    }
    return nullptr;
}
} // namespace

bool add_behaviour_info(const ParserSession &parser, const FeatureSnapshot &features) {
    const auto &syntax = parser.view();
    auto text = semantic::behaviour_info_source(syntax);
    const auto *declaration = module_form(syntax);
    if (text.empty() || !declaration) {
        return false;
    }
    const auto location = syntax.anchor(declaration->source).location;
    SourceManager sources;
    Lexer lexer(sources.add("<predefined>", std::move(text)));
    lexer.set_location(location.file, location.line);
    const auto tokens = lexer.form();
    parser.parse_form(tokens, tokens.back(), features);
    return true;
}
} // namespace clause::cli

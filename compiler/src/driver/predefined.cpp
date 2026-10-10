#include "predefined.hpp"
#include "../codegen/digest.hpp"
#include "../semantic/behaviours.hpp"
#include "../semantic/module_info.hpp"
#include <clause/compiler/lexer.hpp>
#include <clause/compiler/printing.hpp>
#include <sstream>

namespace clause::cli {
namespace {
// The -module form, whose location the generated functions take (OTP gives them no line).
const ast::Form *module_form(const ast::Module &syntax) {
    for (const auto &id : syntax.forms()) {
        if (std::holds_alternative<ast::ModuleAttribute>(syntax.form(id).value)) {
            return &syntax.form(id);
        }
    }
    return nullptr;
}

// What module_info reports beside the syntax; the digest covers the parsed code, so comments and layout do not
// change it while included files do.
semantic::ModuleFacts facts(const ast::Module &syntax, const std::filesystem::path &path, const bool escript) {
    std::ostringstream text;
    print_source(text, syntax);
    const auto absolute = std::filesystem::absolute(path).generic_u8string();
    return {.escript_ = escript,
            .source_ = {absolute.begin(), absolute.end()},
            .version_ = CLAUSE_VERSION,
            .md5_ = codegen::md5(text.str())};
}
} // namespace

std::size_t add_predefined(const ParserSession &parser, const FeatureSnapshot &features,
                           const std::filesystem::path &path, const bool escript) {
    const auto &syntax = parser.view();
    const auto *declaration = module_form(syntax);
    if (!declaration) {
        return 0;
    }
    const auto behaviour_info = semantic::behaviour_info_source(syntax);
    auto text =
        behaviour_info + semantic::module_info_source(syntax, facts(syntax, path, escript), !behaviour_info.empty());
    const auto &origin = syntax.anchor(declaration->source);
    const auto location = origin.location;
    // The buffer takes the module's physical name, so source listings name only real files.
    SourceManager sources;
    Lexer lexer(sources.add(origin.spelling.source ? origin.spelling.source->name : location.file, std::move(text)));
    lexer.set_location(location.file, location.line);
    const auto before = syntax.forms().size();
    for (auto tokens = lexer.form(); !tokens.empty(); tokens = lexer.form()) {
        parser.parse_form(tokens, tokens.back(), features);
    }
    return parser.view().forms().size() - before;
}
} // namespace clause::cli

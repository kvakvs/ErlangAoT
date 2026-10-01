#include "source_locations.hpp"
#include "../semantic/declarations.hpp"
#include <limits>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/Module.h>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Reject unrepresentable line numbers instead of attaching a different source line after narrowing.
unsigned line_number(const Span &site) {
    const auto line = site.source->position(site.begin).line;
    if (line > std::numeric_limits<unsigned>::max()) {
        throw std::length_error("source line exceeds LLVM location limit");
    }
    return static_cast<unsigned>(line);
}

// Preserve the original Erlang identity and physical declaration file in each definition's scope.
void function_scope(llvm::DIBuilder &debug, llvm::Module &output, const semantic::Module &module,
                    const semantic::Function &function, bool optimized, SourceScopes &sources) {
    const auto &site = source_site(module.syntax->anchor(module.syntax->form(function.form).source));
    auto *file = debug.createFile(site.source->name, "");
    auto *type = debug.createSubroutineType(debug.getOrCreateTypeArray({}));
    const auto line = line_number(site);
    const auto flags = llvm::DISubprogram::toSPFlags(false, true, optimized);
    auto *scope = debug.createFunction(file, utf8(function.key.name), function.symbol, file, line, type, line,
                                       llvm::DINode::FlagZero, flags);
    output.getFunction(function.symbol)->setSubprogram(scope);
    for (const auto &origin : module.syntax->extent(module.syntax->form(function.form).source)) {
        const auto &physical = source_site(origin);
        auto *source_file = debug.createFile(physical.source->name, "");
        auto *block = llvm::DILexicalBlockFile::get(output.getContext(), scope, source_file, 0);
        sources.try_emplace(block, physical.source);
    }
}
} // namespace

const Span &source_site(const ast::TokenOrigin &origin) {
    // Macro ancestry consists of definition/invocation pairs, starting with the outer invocation.
    if (origin.related.size() >= 2) {
        return origin.related[1];
    }
    return origin.related.empty() ? origin.spelling : origin.related.front();
}

void prepare_source_locations(llvm::Module &output, const semantic::Module &module, bool optimized,
                              SourceScopes &sources) {
    llvm::DIBuilder debug(output);
    auto *file = debug.createFile(output.getSourceFileName(), "");
    // Erlang uses a private language code here; these scopes describe source lines, not debugger types.
    debug.createCompileUnit(llvm::dwarf::DW_LANG_lo_user, file, "erlangaot", optimized, "", 0, "",
                            llvm::DICompileUnit::LineTablesOnly);
    output.addModuleFlag(llvm::Module::Warning, "Debug Info Version", llvm::DEBUG_METADATA_VERSION);
    for (const auto &function : module.functions) {
        function_scope(debug, output, module, function, optimized, sources);
    }
    debug.finalize();
}

void locate_source(llvm::IRBuilder<> &builder, const ast::Module &syntax, const ast::NodeSource &source) {
    const auto *function = builder.GetInsertBlock()->getParent();
    auto *scope = function->getSubprogram();
    if (!scope) {
        return;
    }
    const auto &site = source_site(syntax.anchor(source));
    auto &context = builder.getContext();
    auto *file = llvm::DIFile::get(context, site.source->name, "");
    auto *location_scope = llvm::DILexicalBlockFile::get(context, scope, file, 0);
    builder.SetCurrentDebugLocation(llvm::DILocation::get(context, line_number(site), 0, location_scope));
}
} // namespace erlang_aot::codegen

#pragma once
#include <clause/compiler/ast/module.hpp>
#include <llvm/IR/IRBuilder.h>
#include <map>

namespace clause::semantic {
struct Module;
}

namespace clause::codegen {
// Retain physical source buffers by LLVM scope identity across optimization and inlining.
using SourceScopes = std::map<const llvm::MDNode *, SourcePtr>;
// Recover physical invocation coordinates independently of logical -file mappings.
const Span &source_site(const ast::TokenOrigin &origin);
// Select the line-table format debuggers of the module's target read: CodeView for MSVC targets, else DWARF.
void request_debug_format(llvm::Module &output);
// Attach line-only function scopes before lowering so LLVM can retain provenance through optimization.
void prepare_source_locations(llvm::Module &output, const semantic::Module &module, bool optimized,
                              SourceScopes &sources);
// Locate the next expression's instructions; absent function scopes keep ordinary code generation unchanged.
void locate_source(llvm::IRBuilder<> &builder, const ast::Module &syntax, const ast::NodeSource &source);
} // namespace clause::codegen

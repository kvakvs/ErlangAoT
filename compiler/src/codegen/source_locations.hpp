#pragma once
#include <erlang_aot/compiler/ast/module.hpp>
#include <llvm/IR/IRBuilder.h>
#include <map>

namespace erlang_aot::semantic {
struct Module;
}

namespace erlang_aot::codegen {
// Retain physical source buffers by LLVM scope identity across optimization and inlining.
using SourceScopes = std::map<const llvm::MDNode *, SourcePtr>;
// Recover physical invocation coordinates independently of logical -file mappings.
const Span &source_site(const ast::TokenOrigin &origin);
// Attach line-only function scopes before lowering so LLVM can retain provenance through optimization.
void prepare_source_locations(llvm::Module &output, const semantic::Module &module, bool optimized,
                              SourceScopes &sources);
// Locate the next expression's instructions; absent function scopes keep ordinary code generation unchanged.
void locate_source(llvm::IRBuilder<> &builder, const ast::Module &syntax, const ast::NodeSource &source);
} // namespace erlang_aot::codegen

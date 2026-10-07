#pragma once
#include "declarations.hpp"
#include <tuple>

namespace erlang_aot::semantic {
// Whether an expression builds a fun: fun F/A, fun M:F/A or an anonymous fun.
bool fun_value(const ast::ExprValue &value);
// Whether a call applies a value (F(Args), (fun f/1)(Args)) instead of naming a function or Module:Function.
bool fun_call(const ast::Module &syntax, const ast::CallExpression &call);
// The function a local fun F/A names, when the module defines it.
const Function *fun_target(const Module &module, const ast::LocalFunReference &reference);
// The module, function and arity of fun M:F/A when all three are literals and the arity is valid.
std::optional<std::tuple<std::u32string, std::u32string, std::size_t>>
external_fun(const ast::RemoteFunReference &reference);
// Number the fun values of every function, in declaration and source order (an anonymous fun before the funs inside
// it); run after binding analysis, which records what anonymous funs capture.
void index_funs(Module &module);
// The entry a fun expression creates.
const FunEntry &fun_entry(const Module &module, const ast::Expression &expression);
} // namespace erlang_aot::semantic

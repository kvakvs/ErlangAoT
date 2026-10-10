#pragma once
#include "declarations.hpp"

namespace clause::semantic {
// Record every -import in Module::imports_, reporting erl_lint's conflicts: a function imported twice, an import
// overriding an auto-imported BIF (warning) and a definition of an imported function.
void index_imports(Module &module, const Reporter &out);
// The module a local call of the function reaches through -import; null when it is not imported.
const std::u32string *import_owner(const Module &module, const FunctionKey &key);
// The modules -import attributes name, so library modules join the batch.
std::vector<std::u32string> import_modules(const ast::Module &syntax);
} // namespace clause::semantic

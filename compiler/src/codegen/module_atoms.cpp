#include "module_atoms.hpp"
#include "../semantic/capabilities.hpp"
#include "../semantic/symbols.hpp"
#include "lowering_state.hpp"
#include <llvm/TargetParser/Triple.h>
#include <set>

namespace erlang_aot::codegen {
namespace {
// Reuse the collision-free symbol encoding for private literal-slot metadata.
std::string slot_name(const std::string &spelling) { return "atom.slot." + semantic::encode_symbol({spelling, "", 0}); }

// Walk only admitted executable children; atom call targets are metadata rather than term expressions.
std::set<std::string> spellings(const semantic::Module &module) {
    std::set<std::string> result;
    std::vector<ast::ExprId> pending;
    for (const auto &function : module.functions) {
        const auto &syntax = std::get<ast::Function>(module.syntax->form(function.form).value);
        pending.push_back(syntax.clauses.at(0).body.at(0));
    }
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = module.syntax->expression(id);
        if (const auto *atom = std::get_if<ast::Atom>(&expression.value)) {
            result.insert(utf8(atom->name));
        }
        const auto children = semantic::expression_children(expression);
        pending.insert(pending.end(), children.begin(), children.end());
    }
    return result;
}
} // namespace

llvm::Constant *emit_atom_table(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
                                std::size_t &count) {
    auto &context = output.getContext();
    auto *type = llvm::StructType::get(llvm::PointerType::get(context, 0), word);
    std::vector<llvm::Constant *> entries;
    for (const auto &name : spellings(module)) {
        auto *bytes = llvm::ConstantDataArray::getString(context, name, false);
        auto *text = new llvm::GlobalVariable(output, bytes->getType(), true, llvm::GlobalValue::PrivateLinkage, bytes,
                                              "atom.spelling");
        auto *slot = llvm::cast<llvm::GlobalVariable>(output.getOrInsertGlobal(slot_name(name), word));
        slot->setConstant(true);
        slot->setLinkage(llvm::GlobalValue::PrivateLinkage);
        slot->setInitializer(llvm::ConstantInt::get(word, entries.size()));
        entries.push_back(llvm::ConstantStruct::get(type, text, llvm::ConstantInt::get(word, name.size())));
    }
    count = entries.size();
    auto *array = llvm::ConstantArray::get(llvm::ArrayType::get(type, count), entries);
    return new llvm::GlobalVariable(output, array->getType(), true, llvm::GlobalValue::PrivateLinkage, array,
                                    "module.atoms");
}

llvm::Value *lower_atom(ExpressionLowering &state, const ast::Atom &atom) {
    auto &output = *state.entry.getParent();
    auto &builder = state.builder;
    auto *slot = output.getNamedGlobal(slot_name(utf8(atom.name)))->getInitializer();
    const auto prefix = semantic::encode_symbol({utf8(state.module.name), "", 0});
    auto *descriptor = output.getNamedGlobal(prefix + ".descriptor");
    const auto &triple = output.getTargetTriple();
    const auto symbol =
        triple.isWindowsMSVCEnvironment()
            ? (triple.isArch64Bit() ? "?erlang_aot_atom_v3@@YA_KPEAX_KPEBX@Z" : "?erlang_aot_atom_v3@@YAIPAXIPBX@Z")
            : (triple.isArch64Bit() ? "_Z18erlang_aot_atom_v3PvmPKv" : "_Z18erlang_aot_atom_v3PvjPKv");
    auto service = output.getOrInsertFunction(
        symbol, llvm::FunctionType::get(state.word, {builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *value = builder.CreateCall(service, {state.entry.getArg(0), slot, descriptor}, "atom.value");
    propagate_failure(state);
    return value;
}
} // namespace erlang_aot::codegen

#include "module_registration.hpp"
#include "../semantic/records.hpp"
#include "../semantic/symbols.hpp"
#include "module_atoms.hpp"
#include "runtime_symbols.hpp"
#include <erlang_aot/abi/v1.hpp>
#include <erlang_aot/compiler/source.hpp>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

namespace erlang_aot::codegen {
namespace {
// Retain exact UTF-8 bytes, including embedded NULs, using explicit lengths in every descriptor.
llvm::Constant *spelling(llvm::Module &output, const std::string &name) {
    auto *bytes = llvm::ConstantDataArray::getString(output.getContext(), name, false);
    return new llvm::GlobalVariable(output, bytes->getType(), true, llvm::GlobalValue::PrivateLinkage, bytes,
                                    "module.spelling");
}

// Store only declared exports; private implementations remain reachable solely through direct calls.
llvm::Constant *exports(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
                        std::size_t &count) {
    auto *ptr = llvm::PointerType::get(output.getContext(), 0);
    auto *type = llvm::StructType::get(ptr, word, word, ptr);
    std::vector<llvm::Constant *> entries;
    for (const auto &function : module.functions) {
        if (!function.exported) {
            continue;
        }
        const auto name = utf8(function.key.name);
        entries.push_back(llvm::ConstantStruct::get(
            type, spelling(output, name), llvm::ConstantInt::get(word, name.size()),
            llvm::ConstantInt::get(word, function.key.arity), output.getFunction(function.symbol)));
    }
    count = entries.size();
    auto *array = llvm::ConstantArray::get(llvm::ArrayType::get(type, count), entries);
    return new llvm::GlobalVariable(output, array->getType(), true, llvm::GlobalValue::PrivateLinkage, array,
                                    "module.exports");
}

// One abi::v1::RecordDescriptor per native record, in semantic::native_layouts order, naming atom slots of this
// module; lowering addresses entries of this `<prefix>.records` table.
llvm::GlobalVariable *records(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
                              llvm::GlobalVariable *descriptor, const std::string &prefix) {
    auto *ptr = llvm::PointerType::get(output.getContext(), 0);
    auto *type = llvm::StructType::get(ptr, word, word, word, ptr, word);
    const auto slot = [&](const ast::Atom &atom) { return atom_slot(output, utf8(atom.name)); };
    std::vector<llvm::Constant *> entries;
    for (const auto *layout : semantic::native_layouts(module)) {
        std::vector<llvm::Constant *> slots;
        slots.reserve(layout->fields.size());
        for (const auto &field : layout->fields) {
            slots.push_back(slot(field.name));
        }
        auto *array = llvm::ConstantArray::get(llvm::ArrayType::get(word, slots.size()), slots);
        auto *fields = new llvm::GlobalVariable(output, array->getType(), true, llvm::GlobalValue::PrivateLinkage,
                                                array, "record.fields");
        const bool exported = module.exported_records.contains(layout->name.name);
        entries.push_back(llvm::ConstantStruct::get(type, descriptor, slot(ast::Atom{module.name}), slot(layout->name),
                                                    llvm::ConstantInt::get(word, exported ? 1 : 0), fields,
                                                    llvm::ConstantInt::get(word, slots.size())));
    }
    auto *array = llvm::ConstantArray::get(llvm::ArrayType::get(type, entries.size()), entries);
    // Other modules of the batch construct exported records from this table.
    return new llvm::GlobalVariable(output, array->getType(), true, llvm::GlobalValue::ExternalLinkage, array,
                                    prefix + ".records");
}
} // namespace

void emit_registration(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word) {
    llvm::IRBuilder<> builder(output.getContext());
    auto *ptr = builder.getPtrTy();
    auto *type =
        llvm::StructType::get(builder.getInt32Ty(), builder.getInt32Ty(), ptr, word, ptr, word, ptr, word, ptr, word);
    const auto name = utf8(module.name);
    std::size_t count = 0;
    auto *table = exports(output, module, word, count);
    std::size_t atom_count = 0;
    auto *atoms = emit_atom_table(output, module, word, atom_count);
    const auto prefix = semantic::encode_symbol({name, "", 0});
    // Records point back at the module descriptor, so it is created before its initializer.
    auto *descriptor = new llvm::GlobalVariable(output, type, true, llvm::GlobalValue::ExternalLinkage, nullptr,
                                                prefix + ".descriptor");
    auto *record_table = records(output, module, word, descriptor, prefix);
    const auto record_count = semantic::native_layouts(module).size();
    descriptor->setInitializer(llvm::ConstantStruct::get(
        type, builder.getInt32(abi::v1::version), builder.getInt32(word->getBitWidth()), spelling(output, name),
        llvm::ConstantInt::get(word, name.size()), table, llvm::ConstantInt::get(word, count), atoms,
        llvm::ConstantInt::get(word, atom_count), record_table, llvm::ConstantInt::get(word, record_count)));
    auto *entry = llvm::Function::Create(llvm::FunctionType::get(builder.getInt8Ty(), {ptr}, false),
                                         llvm::GlobalValue::ExternalLinkage, prefix + ".register", output);
    builder.SetInsertPoint(llvm::BasicBlock::Create(output.getContext(), "entry", entry));
    auto service = output.getOrInsertFunction(services::symbol<services::RegisterModule>(output.getTargetTriple()),
                                              llvm::FunctionType::get(builder.getInt8Ty(), {ptr, ptr}, false));
    builder.CreateRet(builder.CreateCall(service, {entry->getArg(0), descriptor}));
    llvm::appendToUsed(output, {entry, descriptor});
}
} // namespace erlang_aot::codegen

#include "startup.hpp"
#include "../semantic/symbols.hpp"
#include "llvm_state.hpp"
#include "runtime_symbols.hpp"
#include <erlang_aot/abi/startup.hpp>
#include <erlang_aot/compiler/source.hpp>
#include <llvm/IR/IRBuilder.h>

namespace erlang_aot::codegen {
namespace {
// Private constant with exact UTF-8 bytes; the descriptor stores its size separately.
llvm::Constant *spelling(llvm::Module &output, const std::string_view text, const char *name) {
    auto *bytes = llvm::ConstantDataArray::getString(output.getContext(), text, false);
    return new llvm::GlobalVariable(output, bytes->getType(), true, llvm::GlobalValue::PrivateLinkage, bytes, name);
}

// Array of the batch's external module descriptors, in source (registration) order.
llvm::Constant *descriptors(llvm::Module &output, const std::span<const std::unique_ptr<semantic::Module>> modules) {
    auto &context = output.getContext();
    std::vector<llvm::Constant *> entries;
    for (const auto &module : modules) {
        const auto symbol = semantic::encode_symbol({utf8(module->name), "", 0}) + ".descriptor";
        entries.push_back(output.getOrInsertGlobal(symbol, llvm::Type::getInt8Ty(context)));
    }
    auto *array =
        llvm::ConstantArray::get(llvm::ArrayType::get(llvm::PointerType::get(context, 0), entries.size()), entries);
    return new llvm::GlobalVariable(output, array->getType(), true, llvm::GlobalValue::PrivateLinkage, array,
                                    "startup.modules");
}

// Constant abi::v1::StartupDescriptor naming the modules and the arity-1 entry.
llvm::GlobalVariable *descriptor(llvm::Module &output, const std::span<const std::unique_ptr<semantic::Module>> modules,
                                 const StartupRequest &request, llvm::IntegerType *word) {
    llvm::IRBuilder<> builder(output.getContext());
    auto *ptr = builder.getPtrTy();
    auto *i32 = builder.getInt32Ty();
    auto *type = llvm::StructType::get(i32, i32, ptr, word, ptr, word, ptr, word, i32);
    const auto module = utf8(modules[request.module]->name);
    const auto size = [word](const std::size_t value) { return llvm::ConstantInt::get(word, value); };
    auto *data = llvm::ConstantStruct::get(
        type, {builder.getInt32(abi::v1::version), builder.getInt32(word->getBitWidth()), descriptors(output, modules),
               size(modules.size()), spelling(output, module, "startup.module"), size(module.size()),
               spelling(output, request.function, "startup.function"), size(request.function.size()),
               builder.getInt32(request.escript ? abi::v1::startup_escript : 0)});
    return new llvm::GlobalVariable(output, type, true, llvm::GlobalValue::PrivateLinkage, data, "startup.descriptor");
}

// Define `int main(int, char **)` forwarding argc/argv and the descriptor to the runtime's program service.
void define_main(llvm::Module &output, llvm::GlobalVariable *startup) {
    llvm::IRBuilder<> builder(output.getContext());
    auto *ptr = builder.getPtrTy();
    auto *i32 = builder.getInt32Ty();
    auto *main = llvm::Function::Create(llvm::FunctionType::get(i32, {i32, ptr}, false),
                                        llvm::GlobalValue::ExternalLinkage, "main", output);
    builder.SetInsertPoint(llvm::BasicBlock::Create(output.getContext(), "entry", main));
    auto service = output.getOrInsertFunction(services::symbol<services::Main>(output.getTargetTriple()),
                                              llvm::FunctionType::get(i32, {i32, ptr, ptr}, false));
    builder.CreateRet(builder.CreateCall(service, {main->getArg(0), main->getArg(1), startup}));
}
} // namespace

void emit_startup(Compilation &compilation, const std::span<const std::unique_ptr<semantic::Module>> modules,
                  llvm::IntegerType *word) {
    auto &state = detail::state(compilation);
    if (!state.request.startup || state.request.startup->module >= modules.size() || state.modules.empty()) {
        throw std::invalid_argument("startup: entry module outside the batch");
    }
    const auto &request = *state.request.startup;
    auto output = std::make_unique<llvm::Module>("startup", *state.context);
    output->setTargetTriple(state.modules.front()->getTargetTriple());
    output->setDataLayout(state.modules.front()->getDataLayout());
    define_main(*output, descriptor(*output, modules, request, word));
    state.modules.push_back(std::move(output));
}
} // namespace erlang_aot::codegen

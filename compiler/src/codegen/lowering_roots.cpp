#include "../semantic/symbols.hpp"
#include "lowering_state.hpp"
#include "module_atoms.hpp"
#include "runtime_symbols.hpp"
#include <algorithm>
#include <llvm/IR/Module.h>

namespace erlang_aot::codegen {
namespace {
// Name this function in stack traces (abi::v1::FrameDescriptor): module descriptor, module and function name
// atom slots, and arity.
llvm::Constant *frame_descriptor(ExpressionLowering &state) {
    auto &output = *state.entry.getParent();
    const auto module = utf8(state.module.name);
    auto *descriptor = output.getNamedGlobal(semantic::encode_symbol({module, "", 0}) + ".descriptor");
    auto *type = llvm::StructType::get(state.builder.getPtrTy(), state.word, state.word, state.word);
    auto *data = llvm::ConstantStruct::get(type, descriptor, atom_slot(output, module),
                                           atom_slot(output, utf8(state.function.key.name)),
                                           llvm::ConstantInt::get(state.word, state.function.key.arity));
    return new llvm::GlobalVariable(output, type, true, llvm::GlobalValue::PrivateLinkage, data,
                                    "frame." + state.function.symbol);
}
} // namespace

FunctionRoots begin_roots(ExpressionLowering &state) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto service = output.getOrInsertFunction(
        services::symbol<services::RootsEnter>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getPtrTy(), {builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *buffer = builder.CreateCall(
        service, {state.entry.getArg(0), llvm::ConstantInt::get(state.word, 1), frame_descriptor(state)}, "roots");
    propagate_failure(state);
    return {buffer, state.word, state.function.key.arity};
}

llvm::Value *root_slot(ExpressionLowering &state) {
    auto &roots = *state.roots;
    auto *slot = state.builder.CreateGEP(roots.word, roots.buffer, llvm::ConstantInt::get(roots.word, roots.next++),
                                         "root.slot");
    roots.capacity = std::max(roots.capacity, roots.next);
    return slot;
}

void root_value(ExpressionLowering &state, llvm::Value *value) {
    auto *slot = root_slot(state);
    state.builder.CreateAlignedStore(value, slot, llvm::Align(state.word->getBitWidth() / 8));
}

void root_arguments(ExpressionLowering &state) {
    for (std::size_t i = 0; i < state.roots->arguments; ++i) {
        auto *slot = state.builder.CreateGEP(state.word, state.entry.getArg(1), llvm::ConstantInt::get(state.word, i));
        auto *value = state.builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8));
        root_value(state, value);
    }
}

void reset_candidate_roots(ExpressionLowering &state) {
    auto &roots = *state.roots;
    if (roots.next > roots.arguments) {
        auto *start =
            state.builder.CreateGEP(roots.word, roots.buffer, llvm::ConstantInt::get(roots.word, roots.arguments));
        state.builder.CreateMemSet(start, state.builder.getInt8(0),
                                   (roots.next - roots.arguments) * (roots.word->getBitWidth() / 8),
                                   llvm::Align(roots.word->getBitWidth() / 8));
    }
    roots.next = roots.arguments;
}

void finish_roots(ExpressionLowering &state) {
    auto &roots = *state.roots;
    roots.buffer->setArgOperand(1, llvm::ConstantInt::get(roots.word, std::max(roots.capacity, std::size_t{1})));
    auto &output = *state.entry.getParent();
    auto service = output.getOrInsertFunction(
        services::symbol<services::RootsLeave>(output.getTargetTriple()),
        llvm::FunctionType::get(state.builder.getInt8Ty(),
                                {state.builder.getPtrTy(), state.builder.getPtrTy(), roots.word}, false));
    for (auto &block : state.entry) {
        if (auto *ret = llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator())) {
            llvm::IRBuilder<> builder(ret);
            builder.CreateCall(service, {state.entry.getArg(0), roots.buffer, ret->getReturnValue()});
        }
    }
}
} // namespace erlang_aot::codegen

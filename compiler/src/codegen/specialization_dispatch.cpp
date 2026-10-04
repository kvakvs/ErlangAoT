#include "specialization_lowering.hpp"
#include <erlang_aot/abi/v1.hpp>

namespace erlang_aot::codegen {
namespace {
// Test only implemented low tags; no heap access, narrowing, specification promises or unchecked unboxing.
llvm::Value *guard(llvm::IRBuilder<> &builder, llvm::Function &entry, const TypeProfile &profile) {
    auto *word = llvm::cast<llvm::IntegerType>(entry.getReturnType());
    llvm::Value *condition = builder.getTrue();
    for (std::size_t index = 0; index < profile.size(); ++index) {
        if (profile[index] != Representation::small_integer) {
            continue;
        }
        auto *slot = builder.CreateGEP(word, entry.getArg(1), llvm::ConstantInt::get(word, index));
        auto *value = builder.CreateAlignedLoad(word, slot, llvm::Align(word->getBitWidth() / 8));
        auto *tag = llvm::ConstantInt::get(word, abi::v1::small_integer_tag);
        auto *masked = builder.CreateAnd(value, tag);
        auto *check =
            builder.Insert(llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, masked, tag));
        condition = builder.CreateAnd(condition, check);
    }
    return condition;
}

// Forward both ABI pointers unchanged through hit and miss paths.
llvm::CallInst *return_call(llvm::IRBuilder<> &builder, llvm::Function &entry, llvm::Function &target) {
    auto *call = builder.CreateCall(&target, {entry.getArg(0), entry.getArg(1)});
    call->setCallingConv(target.getCallingConv());
    builder.CreateRet(call);
    return call;
}
} // namespace

SpecializedDispatch create_dispatch(llvm::Function &generic, const std::span<const SpecializedVariant> variants) {
    auto *entry = llvm::Function::Create(generic.getFunctionType(), llvm::GlobalValue::InternalLinkage,
                                         generic.getName() + ".dispatch", generic.getParent());
    entry->setCallingConv(generic.getCallingConv());
    llvm::IRBuilder<> builder(llvm::BasicBlock::Create(generic.getContext(), "entry", entry));
    for (const auto &variant : variants) {
        auto *hit = llvm::BasicBlock::Create(generic.getContext(), "type.hit", entry);
        auto *miss = llvm::BasicBlock::Create(generic.getContext(), "type.miss", entry);
        builder.CreateCondBr(guard(builder, *entry, variant.profile), hit, miss);
        builder.SetInsertPoint(hit);
        return_call(builder, *entry, *variant.function);
        builder.SetInsertPoint(miss);
    }
    return {entry, return_call(builder, *entry, generic)};
}
} // namespace erlang_aot::codegen

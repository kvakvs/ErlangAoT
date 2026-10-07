#include "../semantic/funs.hpp"
#include "../semantic/symbols.hpp"
#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <algorithm>
#include <erlang_aot/abi/equality.hpp>
#include <llvm/IR/Module.h>

// Function values through erlang_aot_make_fun_v1 and erlang_aot_apply_v1 (docs/funs.md).
namespace erlang_aot::codegen {
namespace {
// The FunDescriptor of a fun expression in this module's `<prefix>.funs` table.
llvm::Constant *descriptor(ExpressionLowering &state, const ast::Expression &expression) {
    auto &output = *state.entry.getParent();
    const auto name = semantic::encode_symbol({utf8(state.module.name), "", 0}) + ".funs";
    auto *table = output.getNamedGlobal(name);
    if (!table) {
        throw std::invalid_argument("lowering: fun table is missing");
    }
    const auto index = state.module.fun_entries.at(&expression);
    return llvm::ConstantExpr::getInBoundsGetElementPtr(
        table->getValueType(), table,
        llvm::ArrayRef<llvm::Constant *>{llvm::ConstantInt::get(state.word, 0),
                                         llvm::ConstantInt::get(state.word, index)});
}

// A word array holding the call's arguments; lower_frames makes it the process registers, where the fun's captured
// values follow the arguments, so it is never empty.
llvm::Value *arguments(ExpressionLowering &state, const ast::CallExpression &call) {
    auto &builder = state.builder;
    const auto count = std::max<std::size_t>(call.arguments.size(), 1);
    auto *array = builder.CreateAlloca(state.word, llvm::ConstantInt::get(state.word, count), "apply.arguments");
    const llvm::Align alignment(state.word->getBitWidth() / 8);
    array->setAlignment(alignment);
    for (std::size_t i = 0; i < call.arguments.size(); ++i) {
        auto *value = state.values.at(&state.module.syntax->expression(call.arguments[i]));
        builder.CreateAlignedStore(value, builder.CreateGEP(state.word, array, llvm::ConstantInt::get(state.word, i)),
                                   alignment);
    }
    return array;
}
} // namespace

llvm::Value *lower_fun(ExpressionLowering &state, const ast::Expression &expression) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *ptr = builder.getPtrTy();
    auto service = output.getOrInsertFunction(
        services::symbol<services::MakeFun>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(), {ptr, ptr, ptr, state.word, ptr}, false));
    auto *slot = root_slot(state);
    builder.CreateCall(service, {state.entry.getArg(0), descriptor(state, expression),
                                 llvm::ConstantPointerNull::get(ptr), llvm::ConstantInt::get(state.word, 0), slot});
    propagate_failure(state);
    return builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8), "fun.value");
}

llvm::Value *lower_fun_call(ExpressionLowering &state, const ast::Expression &expression,
                            const ast::CallExpression &call) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *ptr = builder.getPtrTy();
    auto *fun = state.values.at(&state.module.syntax->expression(call.target));
    auto *array = arguments(state, call);
    auto apply = output.getOrInsertFunction(services::symbol<services::Apply>(output.getTargetTriple()),
                                            llvm::FunctionType::get(ptr, {ptr, state.word, state.word, ptr}, false));
    auto *frame = builder.CreateCall(
        apply, {state.entry.getArg(0), fun, llvm::ConstantInt::get(state.word, call.arguments.size()), array},
        "apply.frame");
    propagate_failure(state);
    auto marker = output.getOrInsertFunction(APPLY_MARKER, llvm::FunctionType::get(state.word, {ptr, ptr, ptr}, false));
    auto *result = builder.CreateCall(marker, {state.entry.getArg(0), array, frame}, "apply.result");
    if (state.tail_calls && state.tail_calls->contains(&expression)) {
        // The called fun's result is this function's: lower_frames turns the return into a tail transfer.
        builder.CreateRet(result);
        builder.SetInsertPoint(llvm::BasicBlock::Create(state.entry.getContext(), "tail.dead", &state.entry));
        return llvm::ConstantInt::get(state.word, abi::v1::empty_list);
    }
    propagate_failure(state);
    return result;
}
} // namespace erlang_aot::codegen

#include "../semantic/match_plan.hpp"
#include "../semantic/records.hpp"
#include "../semantic/symbols.hpp"
#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <algorithm>
#include <erlang_aot/abi/records.hpp>
#include <llvm/IR/Module.h>
#include <stdexcept>

// Native records through erlang_aot_record_v1 (docs/native-records.md#operations).
namespace erlang_aot::codegen {
namespace {
using Op = abi::v1::RecordOperation;
using Check = abi::v1::RecordCheck;
using Outcome = abi::v1::RecordOutcome;

// Raise badrecord or badfield with the service's payload in a body; a guard or pattern rejects instead.
llvm::BasicBlock *rejection(ExpressionLowering &state, const ServiceOutput result) {
    if (state.rejection) {
        return state.rejection;
    }
    auto &builder = state.builder;
    auto *saved = builder.GetInsertBlock();
    auto &context = state.entry.getContext();
    auto *dispatch = llvm::BasicBlock::Create(context, "record.rejection", &state.entry);
    auto *bad_record = llvm::BasicBlock::Create(context, "record.badrecord", &state.entry);
    auto *bad_field = llvm::BasicBlock::Create(context, "record.badfield", &state.entry);
    builder.SetInsertPoint(dispatch);
    auto *payload = builder.CreateAlignedLoad(state.word, result.slot, llvm::Align(state.word->getBitWidth() / 8));
    auto *test = builder.Insert(llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, result.outcome,
                                                      builder.getInt8(static_cast<std::uint8_t>(Outcome::bad_record))));
    builder.CreateCondBr(test, bad_record, bad_field);
    builder.SetInsertPoint(bad_record);
    raise_reason(state, abi::v1::ErrorReason::badrecord, payload);
    builder.SetInsertPoint(bad_field);
    raise_reason(state, abi::v1::ErrorReason::badfield, payload);
    builder.SetInsertPoint(saved);
    return dispatch;
}

// Call the service with rooted operands and one rooted output slot.
ServiceOutput call(ExpressionLowering &state, Op operation, Check check, llvm::Constant *descriptor,
                   std::span<llvm::Value *const> values) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *arguments = builder.CreateGEP(state.word, state.roots->buffer,
                                        llvm::ConstantInt::get(state.word, state.roots->next), "record.arguments");
    for (auto *value : values) {
        root_value(state, value);
    }
    auto *slot = root_slot(state);
    auto *ptr = builder.getPtrTy();
    auto callee = output.getOrInsertFunction(
        services::symbol<services::Record>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(),
                                {ptr, builder.getInt8Ty(), builder.getInt8Ty(), ptr, ptr, state.word, ptr}, false));
    auto *outcome = builder.CreateCall(callee,
                                       {state.entry.getArg(0), builder.getInt8(static_cast<std::uint8_t>(operation)),
                                        builder.getInt8(static_cast<std::uint8_t>(check)),
                                        descriptor ? descriptor : llvm::ConstantPointerNull::get(ptr), arguments,
                                        llvm::ConstantInt::get(state.word, values.size()), slot},
                                       "record.outcome");
    return {outcome, slot};
}

// The descriptor of a native record of this module inside its module's record table.
llvm::Constant *descriptor(ExpressionLowering &state, const semantic::RecordLayout &layout) {
    const auto layouts = semantic::native_layouts(state.module);
    const auto index = static_cast<std::size_t>(std::ranges::find(layouts, &layout) - layouts.begin());
    auto &output = *state.entry.getParent();
    const auto prefix = semantic::encode_symbol({utf8(state.module.name), "", 0});
    auto *table = output.getNamedGlobal(prefix + ".records");
    if (!table || index == layouts.size()) {
        throw std::invalid_argument("lowering: native record descriptor is missing");
    }
    return llvm::ConstantExpr::getInBoundsGetElementPtr(
        table->getValueType(), table,
        llvm::ArrayRef<llvm::Constant *>{llvm::ConstantInt::get(state.word, 0),
                                         llvm::ConstantInt::get(state.word, index)});
}

// The value of an already evaluated child expression.
llvm::Value *value_of(ExpressionLowering &state, const ast::ExprId &id) {
    return state.values.at(&state.module.syntax->expression(id));
}

// Local construction: every field value in definition order, explicit or default.
llvm::Value *construct(ExpressionLowering &state, const ast::Expression &expression,
                       const semantic::RecordLayout &layout) {
    const auto &values = state.record_values.at(&expression);
    const auto result = call(state, Op::make, Check::any, descriptor(state, layout), values);
    return checked_value(state, result, rejection(state, result));
}

// Local update: values in source order, then the record; the record must be this module's record of the name.
llvm::Value *update(ExpressionLowering &state, const ast::RecordExpression &record,
                    const semantic::RecordLayout &layout) {
    std::vector<llvm::Value *> values{value_of(state, *record.base), lower_atom(state, ast::Atom{state.module.name}),
                                      lower_atom(state, layout.name)};
    for (const auto &field : record.fields) {
        values.push_back(lower_atom(state, std::get<ast::Atom>(field.name)));
        values.push_back(value_of(state, field.value));
    }
    const auto result = call(state, Op::update, Check::module_name, nullptr, values);
    return checked_value(state, result, rejection(state, result));
}

// Local access checks only the record name, as OTP's runtime does.
llvm::Value *access(ExpressionLowering &state, const ast::RecordAccess &access, const semantic::RecordLayout &layout) {
    const std::array values{value_of(state, access.base), lower_atom(state, ast::Atom{state.module.name}),
                            lower_atom(state, layout.name), lower_atom(state, access.field)};
    const auto result = call(state, Op::get, Check::name, nullptr, values);
    return checked_value(state, result, rejection(state, result));
}
} // namespace

llvm::Value *lower_native_record(ExpressionLowering &state, const ast::Expression &expression,
                                 const semantic::RecordLayout &layout) {
    if (const auto *record = std::get_if<ast::RecordExpression>(&expression.value)) {
        return record->base ? update(state, *record, layout) : construct(state, expression, layout);
    }
    return access(state, std::get<ast::RecordAccess>(expression.value), layout);
}

llvm::Value *lower_record_pattern(ExpressionLowering &state, const semantic::MatchNode &node, llvm::Value *input,
                                  llvm::BasicBlock *mismatch) {
    const auto &name = std::get<ast::Atom>(*node.literal);
    if (node.operation == semantic::MatchOperation::record_test) {
        auto *test = lower_native_test(state, static_cast<Check>(node.index), input,
                                       lower_atom(state, *node.record_module), lower_atom(state, name));
        auto *matched = llvm::BasicBlock::Create(state.entry.getContext(), "record.matched", &state.entry);
        state.builder.CreateCondBr(test, matched, mismatch);
        state.builder.SetInsertPoint(matched);
        return input;
    }
    // The check ignores module and name, so any atom of this module fills their operands.
    auto *none = lower_atom(state, ast::Atom{state.module.name});
    const std::array values{input, none, none, lower_atom(state, name)};
    return checked_value(state, call(state, Op::match, Check::any, nullptr, values), mismatch);
}

llvm::Value *lower_native_test(ExpressionLowering &state, abi::v1::RecordCheck check, llvm::Value *value,
                               llvm::Value *module, llvm::Value *name) {
    const std::array values{value, module, name};
    const auto result = call(state, Op::test, check, nullptr, values);
    propagate_failure(state);
    return state.builder.Insert(
        llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, result.outcome,
                              state.builder.getInt8(static_cast<std::uint8_t>(Outcome::success))));
}
} // namespace erlang_aot::codegen

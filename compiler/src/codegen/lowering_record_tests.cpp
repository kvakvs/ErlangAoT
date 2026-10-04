#include "../semantic/capabilities.hpp"
#include "../semantic/records.hpp"
#include "lowering_state.hpp"
#include <erlang_aot/abi/term.hpp>
#include <llvm/Transforms/Utils/SSAUpdater.h>

namespace erlang_aot::codegen {
namespace {
using Op = abi::v1::ImmediateOperation;

// Predicates return canonical atoms; only canonical true follows the success edge.
void branch(ExpressionLowering &state, llvm::Value *value, llvm::Value *truth, const GuardEdges edges) {
    state.builder.CreateCondBr(lower_exact(state, value, truth), edges.success, edges.rejection);
    state.builder.SetInsertPoint(edges.success);
}

// Literal is_record/2 uses declaration arity; dynamic body tags retain the runtime any-arity BIF behavior.
llvm::Value *arity(ExpressionLowering &state, const ast::Expression &expression, const ast::CallExpression &call) {
    if (call.arguments.size() == 3) {
        return state.values.at(&state.module.syntax->expression(call.arguments[2]));
    }
    const auto &tag = state.module.syntax->expression(semantic::ungroup(*state.module.syntax, call.arguments[1]));
    const auto *name = std::get_if<ast::Atom>(&tag.value);
    const auto *layout = name ? semantic::record_layout(state.module, *name, expression.source) : nullptr;
    return layout ? lower_integer(state, std::to_string(layout->fields.size() + 1)) : nullptr;
}

// OTP accepts small integer tuple arities or atom native identities; other values cause badarg.
struct RecordArity {
    // Distinguish the queried size from the canonical boolean used by predicate branches.
    llvm::Value *size;
    llvm::Value *truth;
};

// OTP accepts small integer tuple arities or atom native identities; other values cause badarg.
void validate_size(ExpressionLowering &state, const RecordArity arity, llvm::BasicBlock *no) {
    auto *integer = llvm::BasicBlock::Create(state.entry.getContext(), "record.integer.arity", &state.entry);
    auto *type = lower_immediate(state, Op::is_atom, arity.size);
    state.builder.CreateCondBr(lower_exact(state, type, arity.truth), no, integer);
    state.builder.SetInsertPoint(integer);
    auto *masked = state.builder.CreateAnd(arity.size, llvm::ConstantInt::get(state.word, abi::v1::small_integer_tag));
    auto *small =
        state.builder.Insert(llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, masked,
                                                   llvm::ConstantInt::get(state.word, abi::v1::small_integer_tag)));
    auto *valid = llvm::BasicBlock::Create(state.entry.getContext(), "record.valid.arity", &state.entry);
    state.builder.CreateCondBr(small, valid, bad_argument_exit(state));
    state.builder.SetInsertPoint(valid);
}

struct RecordResults {
    // Join the checked tag comparison with false outcomes without carrying a candidate-only fact.
    llvm::Value *equal;
    llvm::BasicBlock *yes;
    llvm::Value *falsehood;
    llvm::BasicBlock *no;
};

// SSA formation inspects successors, so temporarily terminate the current unfinished merge block.
llvm::Value *joined(ExpressionLowering &state, RecordResults results) {
    auto *merge = state.builder.GetInsertBlock();
    auto *boundary = state.builder.CreateUnreachable();
    llvm::SmallVector<llvm::PHINode *, 2> phis;
    llvm::SSAUpdater updater(&phis);
    updater.Initialize(state.word, "record.test");
    updater.AddAvailableValue(results.yes, results.equal);
    updater.AddAvailableValue(results.no, results.falsehood);
    auto *result = updater.GetValueInMiddleOfBlock(merge);
    for (auto *phi : phis) {
        phi->setDebugLoc(state.builder.getCurrentDebugLocation());
    }
    boundary->eraseFromParent();
    return result;
}
} // namespace

llvm::Value *lower_record_test(ExpressionLowering &state, const ast::Expression &expression,
                               const ast::CallExpression &call) {
    auto *value = state.values.at(&state.module.syntax->expression(call.arguments[0]));
    auto *tag = state.values.at(&state.module.syntax->expression(call.arguments[1]));
    auto *size = arity(state, expression, call);
    auto *truth = lower_atom(state, ast::Atom{U"true"});
    auto *falsehood = lower_atom(state, ast::Atom{U"false"});
    auto &context = state.entry.getContext();
    auto *no = llvm::BasicBlock::Create(context, "record.false", &state.entry);
    auto *merge = llvm::BasicBlock::Create(context, "record.result", &state.entry);
    auto *valid_tag = llvm::BasicBlock::Create(context, "record.valid.tag", &state.entry);
    branch(state, lower_immediate(state, Op::is_atom, tag), truth, {valid_tag, bad_argument_exit(state)});
    if (size) {
        validate_size(state, {size, truth}, no);
    }
    auto *tuple = llvm::BasicBlock::Create(context, "record.tuple", &state.entry);
    branch(state, lower_immediate(state, Op::is_tuple, value), truth, {tuple, no});
    auto *count = lower_immediate(state, Op::tuple_size, value);
    auto *nonempty = llvm::BasicBlock::Create(context, "record.nonempty", &state.entry);
    branch(state, lower_operation(state, Op::greater, count, lower_integer(state, "0")), truth, {nonempty, no});
    if (size) {
        auto *shape = llvm::BasicBlock::Create(context, "record.shape", &state.entry);
        branch(state, lower_immediate(state, Op::exact_equal, count, size), truth, {shape, no});
    }
    auto *first = lower_inspection(state, abi::v1::ContainerInspection::tuple_element, value, 0, no);
    auto *equal = lower_immediate(state, Op::exact_equal, first, tag);
    auto *yes_path = state.builder.GetInsertBlock();
    state.builder.CreateBr(merge);
    state.builder.SetInsertPoint(no);
    state.builder.CreateBr(merge);
    state.builder.SetInsertPoint(merge);
    return joined(state, {equal, yes_path, falsehood, no});
}

llvm::Value *lower_integer_range(ExpressionLowering &state, const ast::CallExpression &call) {
    auto *value = state.values.at(&state.module.syntax->expression(call.arguments[0]));
    auto *lower = state.values.at(&state.module.syntax->expression(call.arguments[1]));
    auto *upper = state.values.at(&state.module.syntax->expression(call.arguments[2]));
    auto *truth = lower_atom(state, ast::Atom{U"true"});
    auto *falsehood = lower_atom(state, ast::Atom{U"false"});
    auto &context = state.entry.getContext();
    auto *valid = llvm::BasicBlock::Create(context, "range.valid.bounds", &state.entry);
    auto *lower_type = lower_immediate(state, Op::is_integer, lower);
    auto *upper_type = lower_immediate(state, Op::is_integer, upper);
    auto *bounds = lower_immediate(state, Op::logical_and, lower_type, upper_type);
    branch(state, bounds, truth, {valid, bad_argument_exit(state)});
    auto *no = llvm::BasicBlock::Create(context, "range.false", &state.entry);
    auto *integer = llvm::BasicBlock::Create(context, "range.integer", &state.entry);
    auto *merge = llvm::BasicBlock::Create(context, "range.result", &state.entry);
    branch(state, lower_immediate(state, Op::is_integer, value), truth, {integer, no});
    auto *above = lower_immediate(state, Op::greater_equal, value, lower);
    auto *below = lower_immediate(state, Op::less_equal, value, upper);
    auto *result = lower_immediate(state, Op::logical_and, above, below);
    auto *yes_path = state.builder.GetInsertBlock();
    state.builder.CreateBr(merge);
    state.builder.SetInsertPoint(no);
    state.builder.CreateBr(merge);
    state.builder.SetInsertPoint(merge);
    return joined(state, {result, yes_path, falsehood, no});
}
} // namespace erlang_aot::codegen

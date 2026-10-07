#include "../semantic/capabilities.hpp"
#include "../semantic/match_plan.hpp"
#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include "source_locations.hpp"
#include <algorithm>
#include <erlang_aot/abi/calls.hpp>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <llvm/IR/Module.h>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Load canonical target-width literals; atom words always come from runtime module bindings.
llvm::Value *literal(ExpressionLowering &state, const semantic::MatchLiteral &literal) {
    if (const auto *real = std::get_if<ast::FloatLiteral>(&literal)) {
        return lower_float(state, real->value);
    }
    if (const auto *integer = std::get_if<ast::IntegerLiteral>(&literal)) {
        return lower_integer(state, integer->value.decimal);
    }
    if (const auto *atom = std::get_if<ast::Atom>(&literal)) {
        return lower_atom(state, *atom);
    }
    if (const auto *empty = std::get_if<semantic::EmptyValue>(&literal)) {
        return llvm::ConstantInt::get(state.word, *empty == semantic::EmptyValue::tuple ? abi::v1::empty_tuple
                                                                                        : abi::v1::empty_list);
    }
    const auto number = std::get<std::int64_t>(literal);
    const auto encoded = state.word->getBitWidth() == 32 ? *abi::v1::IntegerEncoding<32>::encode(number)
                                                         : *abi::v1::IntegerEncoding<64>::encode(number);
    return llvm::ConstantInt::get(state.word, encoded);
}

// Original candidate arguments are loaded once; checked extractions populate separate SSA slots.
std::vector<llvm::Value *> inputs(ExpressionLowering &state, const semantic::MatchPlan &plan) {
    std::vector<llvm::Value *> values;
    for (std::size_t i = 0; i < plan.inputs; ++i) {
        auto *slot = state.builder.CreateGEP(state.word, state.entry.getArg(1), llvm::ConstantInt::get(state.word, i));
        values.push_back(state.builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8),
                                                         "match.input"));
    }
    return values;
}

// Map plan extraction operations to the shared ownership-checking runtime service.
std::optional<abi::v1::ContainerInspection> inspection(const semantic::MatchOperation operation) {
    using Operation = semantic::MatchOperation;
    using Inspect = abi::v1::ContainerInspection;
    switch (operation) {
    case Operation::tuple_shape:
        return Inspect::tuple_shape;
    case Operation::tuple_element:
        return Inspect::tuple_element;
    case Operation::cons_shape:
        return Inspect::cons_shape;
    case Operation::cons_head:
        return Inspect::cons_head;
    case Operation::cons_tail:
        return Inspect::cons_tail;
    default:
        return {};
    }
}

// The projection shortcut is valid only when no shape, extraction or equality operation can reject.
bool constrained(const semantic::MatchPlan &plan) {
    return std::ranges::any_of(plan.nodes, [](const auto &node) {
        return node.operation != semantic::MatchOperation::bind &&
               node.operation != semantic::MatchOperation::success &&
               node.operation != semantic::MatchOperation::mismatch;
    });
}

// Shape checks preserve the candidate; extraction services populate a separate candidate slot.
bool extracted(const semantic::MatchOperation operation) {
    using Op = semantic::MatchOperation;
    return operation == Op::tuple_element || operation == Op::cons_head || operation == Op::cons_tail ||
           operation == Op::map_lookup || operation == Op::record_field;
}

// Binary services carry both a checked extracted value and a distinct following bit cursor.
bool binary_extraction(ExpressionLowering &state, const semantic::MatchNode &node, std::span<llvm::Value *> values,
                       const std::vector<llvm::BasicBlock *> &blocks) {
    if (node.operation >= semantic::MatchOperation::binary_start &&
        node.operation <= semantic::MatchOperation::binary_finish) {
        const auto result = lower_bit_pattern(state, node, values, blocks.at(node.mismatch));
        if (node.operation != semantic::MatchOperation::binary_finish) {
            values[node.cursor_output] = result.cursor;
        }
        if (node.operation == semantic::MatchOperation::binary_extract) {
            values[node.output] = result.value;
        }
        state.builder.CreateBr(blocks.at(node.success));
        return true;
    }
    return false;
}

// Keep runtime shape/lookup emission independent from scalar binding and exact-equality constraints.
bool extraction(ExpressionLowering &state, const semantic::MatchNode &node, std::span<llvm::Value *> values,
                const std::vector<llvm::BasicBlock *> &blocks) {
    if (binary_extraction(state, node, values, blocks)) {
        return true;
    }
    llvm::Value *result = nullptr;
    if (const auto operation = inspection(node.operation)) {
        result = lower_inspection(state, *operation, values[node.input], node.index, blocks.at(node.mismatch));
    } else if (node.operation == semantic::MatchOperation::map_shape ||
               node.operation == semantic::MatchOperation::map_lookup) {
        result = lower_map_pattern(state, node, values[node.input], blocks.at(node.mismatch));
    } else if (node.operation == semantic::MatchOperation::record_test ||
               node.operation == semantic::MatchOperation::record_field) {
        result = lower_record_pattern(state, node, values[node.input], blocks.at(node.mismatch));
    }
    if (!result) {
        return false;
    }
    if (extracted(node.operation)) {
        values[node.output] = result;
    }
    state.builder.CreateBr(blocks.at(node.success));
    return true;
}

// Bindings remain tentative; every failed constraint reaches the caller-owned mismatch continuation.
void node(ExpressionLowering &state, const semantic::MatchNode &node, const std::span<llvm::Value *> values,
          const std::vector<llvm::BasicBlock *> &blocks) {
    locate_source(state.builder, *state.module.syntax, state.module.syntax->expression(node.source).source);
    if (node.input >= values.size()) {
        throw std::invalid_argument("lowering: match input is out of range");
    }
    if (extraction(state, node, values, blocks)) {
        return;
    }
    auto *input = values[node.input];
    if (node.operation == semantic::MatchOperation::bind) {
        state.bindings.emplace(*node.binding, input);
        state.builder.CreateBr(blocks.at(node.success));
        return;
    }
    auto *expected = node.operation == semantic::MatchOperation::exact_binding ? state.bindings.at(*node.binding)
                                                                               : literal(state, *node.literal);
    state.builder.CreateCondBr(lower_exact(state, input, expected), blocks.at(node.success), blocks.at(node.mismatch));
}

// The head plan of the current candidate: a clause of the function, or of the anonymous fun being lowered.
std::optional<semantic::MatchPlan> head_plan(const ExpressionLowering &state) {
    const auto out = [](const Diagnostic &diagnostic) { throw std::invalid_argument(render(diagnostic)); };
    const semantic::MatchOptions options{.clause = state.clause, .word_bits = state.word->getBitWidth()};
    if (state.lambda) {
        const auto &clauses = *semantic::fun_clauses(state.lambda->expression->value);
        return semantic::make_match_plan(state.module, state.function, clauses.at(state.clause), out, options);
    }
    return semantic::make_match_plan(state.module, state.function, out, options);
}
} // namespace

bool lower_unconditional_head(ExpressionLowering &state) {
    const auto plan = head_plan(state);
    if (!plan) {
        throw std::invalid_argument("lowering: unavailable match plan");
    }
    if (constrained(*plan)) {
        return false;
    }
    std::map<semantic::BindingId, std::size_t> definitions;
    for (const auto &node : plan->nodes) {
        if (node.binding) {
            definitions.emplace(*node.binding, node.input);
        }
    }
    for (const auto &use : state.function.bindings) {
        if (use.use == semantic::BindingUse::definition || !definitions.contains(use.identity) ||
            state.bindings.contains(use.identity)) {
            continue;
        }
        const auto input = definitions.at(use.identity);
        locate_source(state.builder, *state.module.syntax, state.module.syntax->expression(use.expression).source);
        auto *slot = state.builder.CreateGEP(state.word, state.entry.getArg(1),
                                             llvm::ConstantInt::get(state.word, input), "argument.slot");
        state.bindings.emplace(
            use.identity,
            state.builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8), "argument"));
    }
    return true;
}

llvm::Value *lower_exact(ExpressionLowering &state, llvm::Value *left, llvm::Value *right) {
    auto &output = *state.entry.getParent();
    auto service = output.getOrInsertFunction(
        services::symbol<services::Exact>(output.getTargetTriple()),
        llvm::FunctionType::get(state.builder.getInt8Ty(), {state.builder.getPtrTy(), state.word, state.word}, false));
    auto *result = state.builder.CreateCall(service, {state.entry.getArg(0), left, right}, "exact.outcome");
    propagate_failure(state);
    return state.builder.Insert(
        llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, result,
                              state.builder.getInt8(static_cast<std::uint8_t>(abi::v1::Equality::equal))));
}

void lower_head(ExpressionLowering &state, llvm::BasicBlock *success, llvm::BasicBlock *mismatch) {
    const auto plan = head_plan(state);
    if (!plan) {
        throw std::invalid_argument("lowering: unavailable match plan");
    }
    lower_match_plan(state, *plan, inputs(state, *plan), success, mismatch);
}

std::vector<llvm::Value *> lower_match_plan(ExpressionLowering &state, const semantic::MatchPlan &plan,
                                            std::span<llvm::Value *const> values, llvm::BasicBlock *success,
                                            llvm::BasicBlock *mismatch) {
    std::vector<llvm::Value *> candidates(plan.values);
    std::ranges::copy(values, candidates.begin());
    std::vector<llvm::BasicBlock *> blocks;
    for (const auto &node : plan.nodes) {
        if (node.operation == semantic::MatchOperation::success) {
            blocks.push_back(success);
        } else if (node.operation == semantic::MatchOperation::mismatch) {
            blocks.push_back(mismatch);
        } else {
            blocks.push_back(llvm::BasicBlock::Create(state.entry.getContext(), "match.test", &state.entry));
        }
    }
    state.builder.CreateBr(blocks.front());
    for (std::size_t i = 0; i + 2 < plan.nodes.size(); ++i) {
        state.builder.SetInsertPoint(blocks[i]);
        node(state, plan.nodes[i], candidates, blocks);
    }
    return candidates;
}

// A raised exception continues at the innermost handler, or leaves the function through its checked exit.
void unwind(ExpressionLowering &state) {
    if (state.handler) {
        state.builder.CreateBr(state.handler);
    } else {
        state.builder.CreateRet(llvm::ConstantInt::get(state.word, 0));
    }
}

void raise_reason(ExpressionLowering &state, abi::v1::ErrorReason reason, llvm::Value *payload) {
    auto &output = *state.entry.getParent();
    const auto symbol = services::symbol<services::Raise>(output.getTargetTriple());
    auto service = output.getOrInsertFunction(
        symbol, llvm::FunctionType::get(state.builder.getInt8Ty(),
                                        {state.builder.getPtrTy(), state.builder.getInt8Ty(), state.word}, false));
    state.builder.CreateCall(service, {state.entry.getArg(0), state.builder.getInt8(static_cast<std::uint8_t>(reason)),
                                       payload ? payload : llvm::ConstantInt::get(state.word, 0)});
    unwind(state);
}

namespace {
// Declare erlang_aot_reraise_v2, shared by unmatched try handlers and erlang:raise/3.
llvm::FunctionCallee reraise_service(ExpressionLowering &state) {
    auto &output = *state.entry.getParent();
    return output.getOrInsertFunction(
        services::symbol<services::Reraise>(output.getTargetTriple()),
        llvm::FunctionType::get(state.builder.getInt8Ty(),
                                {state.builder.getPtrTy(), state.word, state.word, state.word}, false));
}
} // namespace

void reraise(ExpressionLowering &state, const Exception &exception) {
    state.builder.CreateCall(reraise_service(state), {state.entry.getArg(0), exception[0], exception[1], exception[2]});
    unwind(state);
}

llvm::Value *lower_raise_stack(ExpressionLowering &state, const std::span<llvm::Value *const> arguments) {
    state.builder.CreateCall(reraise_service(state), {state.entry.getArg(0), arguments[0], arguments[1], arguments[2]},
                             "raise.outcome");
    // A recorded exception leaves through this check; an invalid class or stack records nothing.
    propagate_failure(state);
    return lower_atom(state, ast::Atom{U"badarg"});
}

llvm::Value *lower_error(ExpressionLowering &state, llvm::Value *reason, llvm::Value *arguments) {
    auto &output = *state.entry.getParent();
    auto service = output.getOrInsertFunction(
        services::symbol<services::Error>(output.getTargetTriple()),
        llvm::FunctionType::get(state.builder.getInt8Ty(), {state.builder.getPtrTy(), state.word, state.word}, false));
    state.builder.CreateCall(service, {state.entry.getArg(0), reason, arguments}, "error.outcome");
    // The service always records the exception or an infrastructure failure, so this check always unwinds.
    propagate_failure(state);
    return llvm::ConstantInt::get(state.word, abi::v1::empty_list);
}

void raise_function_clause(ExpressionLowering &state) { raise_reason(state, abi::v1::ErrorReason::function_clause); }
} // namespace erlang_aot::codegen

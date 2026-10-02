#include "../semantic/match_plan.hpp"
#include "lowering_state.hpp"
#include "source_locations.hpp"
#include <algorithm>
#include <erlang_aot/abi/calls.hpp>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Triple.h>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Native C++ service linkage uses the emitted target's convention and integer width.
std::string_view exact_symbol(const llvm::Triple &triple) {
    if (triple.isWindowsMSVCEnvironment()) {
        return triple.isArch64Bit() ? "?erlang_aot_exact_v1@@YAEPEAX_K1@Z" : "?erlang_aot_exact_v1@@YAEPAXII@Z";
    }
    return triple.isArch64Bit() ? "_Z19erlang_aot_exact_v1Pvmm" : "_Z19erlang_aot_exact_v1Pvjj";
}

// Load canonical target-width literals; atom words always come from runtime module bindings.
llvm::Value *literal(ExpressionLowering &state, const semantic::MatchLiteral &literal) {
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

// Inputs remain unchanged and are loaded once; no representation-dependent extraction is admitted yet.
std::vector<llvm::Value *> inputs(ExpressionLowering &state, const semantic::MatchPlan &plan) {
    std::vector<llvm::Value *> values;
    for (std::size_t i = 0; i < plan.inputs; ++i) {
        auto *slot = state.builder.CreateGEP(state.word, state.entry.getArg(1), llvm::ConstantInt::get(state.word, i));
        values.push_back(state.builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8),
                                                         "match.input"));
    }
    return values;
}

// Binding values are tentative SSA definitions; every exact test has explicit caller-owned mismatch edges.
void node(ExpressionLowering &state, const semantic::MatchNode &node, const std::vector<llvm::Value *> &values,
          const std::vector<llvm::BasicBlock *> &blocks) {
    locate_source(state.builder, *state.module.syntax, state.module.syntax->expression(node.source).source);
    auto *input = values.at(node.input);
    if (node.operation == semantic::MatchOperation::bind) {
        state.bindings.emplace(*node.binding, input);
        state.builder.CreateBr(blocks.at(node.success));
        return;
    }
    auto *expected = node.operation == semantic::MatchOperation::exact_binding ? state.bindings.at(*node.binding)
                                                                               : literal(state, *node.literal);
    state.builder.CreateCondBr(lower_exact(state, input, expected), blocks.at(node.success), blocks.at(node.mismatch));
}
} // namespace

bool lower_unconditional_head(ExpressionLowering &state) {
    const auto plan = semantic::make_match_plan(state.module, state.function,
                                                [](const Diagnostic &d) { throw std::invalid_argument(render(d)); },
                                                {.clause = state.clause, .word_bits = state.word->getBitWidth()});
    if (!plan) {
        throw std::invalid_argument("lowering: unavailable match plan");
    }
    const auto tests = std::ranges::any_of(plan->nodes, [](const auto &node) {
        return node.operation == semantic::MatchOperation::exact_binding ||
               node.operation == semantic::MatchOperation::exact_literal;
    });
    if (tests) {
        return false;
    }
    std::map<semantic::BindingId, std::size_t> definitions;
    for (const auto &node : plan->nodes) {
        if (node.binding) {
            definitions.emplace(*node.binding, node.input);
        }
    }
    for (const auto &use : state.function.bindings) {
        if (use.use != semantic::BindingUse::read || !definitions.contains(use.identity) ||
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
        exact_symbol(output.getTargetTriple()),
        llvm::FunctionType::get(state.builder.getInt8Ty(), {state.builder.getPtrTy(), state.word, state.word}, false));
    auto *result = state.builder.CreateCall(service, {state.entry.getArg(0), left, right}, "exact.outcome");
    propagate_failure(state);
    return state.builder.Insert(
        llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_EQ, result,
                              state.builder.getInt8(static_cast<std::uint8_t>(abi::v1::Equality::equal))));
}

void lower_head(ExpressionLowering &state, llvm::BasicBlock *success, llvm::BasicBlock *mismatch) {
    const auto plan =
        semantic::make_match_plan(state.module, state.function,
                                  [](const Diagnostic &diagnostic) { throw std::invalid_argument(render(diagnostic)); },
                                  {.clause = state.clause, .word_bits = state.word->getBitWidth()});
    if (!plan) {
        throw std::invalid_argument("lowering: unavailable match plan");
    }
    const auto values = inputs(state, *plan);
    std::vector<llvm::BasicBlock *> blocks;
    for (const auto &node : plan->nodes) {
        if (node.operation == semantic::MatchOperation::success) {
            blocks.push_back(success);
        } else if (node.operation == semantic::MatchOperation::mismatch) {
            blocks.push_back(mismatch);
        } else {
            blocks.push_back(llvm::BasicBlock::Create(state.entry.getContext(), "match.test", &state.entry));
        }
    }
    state.builder.CreateBr(blocks.front());
    for (std::size_t i = 0; i + 2 < plan->nodes.size(); ++i) {
        state.builder.SetInsertPoint(blocks[i]);
        node(state, plan->nodes[i], values, blocks);
    }
}

void raise_reason(ExpressionLowering &state, abi::v1::ErrorReason reason, llvm::Value *payload) {
    auto &output = *state.entry.getParent();
    const auto &triple = output.getTargetTriple();
    const auto symbol =
        triple.isWindowsMSVCEnvironment()
            ? (triple.isArch64Bit() ? "?erlang_aot_raise_v2@@YAEPEAXW4ErrorReason@v1@abi@erlang_aot@@_K@Z"
                                    : "?erlang_aot_raise_v2@@YAEPAXW4ErrorReason@v1@abi@erlang_aot@@I@Z")
            : (triple.isArch64Bit() ? "_Z19erlang_aot_raise_v2PvN10erlang_aot3abi2v111ErrorReasonEm"
                                    : "_Z19erlang_aot_raise_v2PvN10erlang_aot3abi2v111ErrorReasonEj");
    auto service = output.getOrInsertFunction(
        symbol, llvm::FunctionType::get(state.builder.getInt8Ty(),
                                        {state.builder.getPtrTy(), state.builder.getInt8Ty(), state.word}, false));
    state.builder.CreateCall(service, {state.entry.getArg(0), state.builder.getInt8(static_cast<std::uint8_t>(reason)),
                                       payload ? payload : llvm::ConstantInt::get(state.word, 0)});
    state.builder.CreateRet(llvm::ConstantInt::get(state.word, 0));
}

void raise_function_clause(ExpressionLowering &state) { raise_reason(state, abi::v1::ErrorReason::function_clause); }
} // namespace erlang_aot::codegen

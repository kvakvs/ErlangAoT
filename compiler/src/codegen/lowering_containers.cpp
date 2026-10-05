#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <array>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/term.hpp>
#include <llvm/IR/Module.h>

namespace erlang_aot::codegen {
namespace {
// Large constructors use bounded runtime scratch roots, keeping source width off the native stack.
llvm::Value *construct(ExpressionLowering &state, abi::v1::ContainerConstruction operation,
                       const std::span<llvm::Value *const> values) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *arguments = builder.CreateGEP(state.word, state.roots->buffer,
                                        llvm::ConstantInt::get(state.word, state.roots->next), "container.arguments");
    for (auto *value : values) {
        root_value(state, value);
    }
    auto *slot = root_slot(state);
    auto service = output.getOrInsertFunction(
        services::symbol<services::Construct>(output.getTargetTriple()),
        llvm::FunctionType::get(
            builder.getInt8Ty(),
            {builder.getPtrTy(), builder.getInt8Ty(), builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *outcome = builder.CreateCall(service,
                                       {state.entry.getArg(0), builder.getInt8(static_cast<std::uint8_t>(operation)),
                                        arguments, llvm::ConstantInt::get(state.word, values.size()), slot},
                                       "container.outcome");
    return checked_value(state, {outcome, slot}, bad_argument_exit(state));
}

// Values already exist in SSA after the common iterative expression walk visits every child.
std::vector<llvm::Value *> children(ExpressionLowering &state, const std::span<const ast::ExprId> ids) {
    std::vector<llvm::Value *> result;
    result.reserve(ids.size() + 1);
    for (const auto &id : ids) {
        result.push_back(state.values.at(&state.module.syntax->expression(id)));
    }
    return result;
}

// Strings are lists of decoded Unicode integers; no separate runtime string representation exists.
llvm::Value *string(ExpressionLowering &state, const ast::StringLiteral &literal) {
    if (literal.value.empty()) {
        return llvm::ConstantInt::get(state.word, abi::v1::empty_list);
    }
    std::vector<llvm::Value *> values;
    values.reserve(literal.value.size() + 1);
    for (const auto character : literal.value) {
        const auto encoded = *abi::v1::IntegerEncoding<32>::encode(static_cast<std::int64_t>(character));
        values.push_back(llvm::ConstantInt::get(state.word, encoded));
    }
    values.push_back(llvm::ConstantInt::get(state.word, abi::v1::empty_list));
    return construct(state, abi::v1::ContainerConstruction::list, values);
}
} // namespace

llvm::Value *lower_tuple(ExpressionLowering &state, const std::span<llvm::Value *const> values) {
    return construct(state, abi::v1::ContainerConstruction::tuple, values);
}

llvm::Value *lower_list(ExpressionLowering &state, const std::span<llvm::Value *const> values) {
    return construct(state, abi::v1::ContainerConstruction::list, values);
}

llvm::Value *lower_reverse(ExpressionLowering &state, llvm::Value *list) {
    return construct(
        state, abi::v1::ContainerConstruction::reverse,
        std::array{list, static_cast<llvm::Value *>(llvm::ConstantInt::get(state.word, abi::v1::empty_list))});
}

llvm::Value *lower_container(ExpressionLowering &state, const ast::ExprValue &value) {
    if (const auto *tuple = std::get_if<ast::Tuple>(&value)) {
        if (tuple->elements.empty()) {
            return llvm::ConstantInt::get(state.word, abi::v1::empty_tuple);
        }
        return construct(state, abi::v1::ContainerConstruction::tuple, children(state, tuple->elements));
    }
    if (const auto *list = std::get_if<ast::List>(&value)) {
        if (list->elements.empty() && !list->tail) {
            return llvm::ConstantInt::get(state.word, abi::v1::empty_list);
        }
        auto values = children(state, list->elements);
        values.push_back(list->tail ? state.values.at(&state.module.syntax->expression(*list->tail))
                                    : llvm::ConstantInt::get(state.word, abi::v1::empty_list));
        return construct(state, abi::v1::ContainerConstruction::list, values);
    }
    if (const auto *literal = std::get_if<ast::StringLiteral>(&value)) {
        return string(state, *literal);
    }
    return nullptr;
}

llvm::Value *lower_inspection(ExpressionLowering &state, abi::v1::ContainerInspection operation, llvm::Value *value,
                              const std::size_t index, llvm::BasicBlock *mismatch) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *slot = root_slot(state);
    auto service = output.getOrInsertFunction(
        services::symbol<services::Inspect>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(),
                                {builder.getPtrTy(), builder.getInt8Ty(), state.word, state.word, builder.getPtrTy()},
                                false));
    auto *outcome = builder.CreateCall(service,
                                       {state.entry.getArg(0), builder.getInt8(static_cast<std::uint8_t>(operation)),
                                        value, llvm::ConstantInt::get(state.word, index), slot},
                                       "inspect.outcome");
    return checked_value(state, {outcome, slot}, mismatch);
}
} // namespace erlang_aot::codegen

#include "../semantic/binary_options.hpp"
#include "../semantic/capabilities.hpp"
#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <erlang_aot/abi/bits.hpp>
#include <erlang_aot/abi/term.hpp>
#include <llvm/IR/Module.h>
#include <utility>

namespace erlang_aot::codegen {
namespace {
using Op = abi::v1::BitOperation;

// Metadata and cursors fit both target small-integer ranges; no raw host term words enter IR.
llvm::Value *integer(ExpressionLowering &state, const unsigned value) {
    return llvm::ConstantInt::get(state.word, *abi::v1::IntegerEncoding<32>::encode(value));
}

// Resolve native endian from the LLVM data layout, including foreign object emission.
llvm::Value *descriptor(ExpressionLowering &state, const semantic::BinaryOptions &options, const bool empty = false) {
    const bool little = options.native ? state.entry.getParent()->getDataLayout().isLittleEndian() : options.little;
    const auto flags = (empty ? abi::v1::bit_empty : 0) | (little ? abi::v1::bit_little : 0) |
                       (options.signed_value ? abi::v1::bit_signed : 0) | (options.all ? abi::v1::bit_all : 0);
    return integer(state, (options.unit << 8) | flags | static_cast<unsigned>(options.type));
}

// Root every borrowed argument and both success-only outputs through the shared runtime scratch buffer.
BitLowering service(ExpressionLowering &state, Op operation, const std::span<llvm::Value *const> values) {
    auto &builder = state.builder;
    auto &module = *state.entry.getParent();
    auto *arguments = builder.CreateGEP(state.word, state.roots->buffer,
                                        llvm::ConstantInt::get(state.word, state.roots->next), "binary.arguments");
    for (auto *value : values) {
        root_value(state, value);
    }
    auto *slot = root_slot(state);
    auto *next = root_slot(state);
    auto callee = module.getOrInsertFunction(
        services::symbol<services::Bits>(module.getTargetTriple()),
        llvm::FunctionType::get(
            builder.getInt8Ty(),
            {builder.getPtrTy(), builder.getInt8Ty(), builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *outcome = builder.CreateCall(callee,
                                       {state.entry.getArg(0), builder.getInt8(static_cast<std::uint8_t>(operation)),
                                        arguments, llvm::ConstantInt::get(state.word, values.size()), slot},
                                       "binary.outcome");
    auto *value = checked_value(state, {outcome, slot}, bad_argument_exit(state));
    return {value,
            builder.CreateAlignedLoad(state.word, next, llvm::Align(state.word->getBitWidth() / 8), "binary.cursor")};
}

// Expand literal strings as scalar segment values without constructing an unrelated list value.
void segment_values(ExpressionLowering &state, const ast::BinarySegment &segment, std::vector<llvm::Value *> &values) {
    const auto &syntax = *state.module.syntax;
    const auto options = semantic::binary_options(segment);
    auto *metadata = descriptor(state, options);
    auto *size = segment.size ? state.values.at(&syntax.expression(*segment.size)) : integer(state, options.size);
    const auto &value = syntax.expression(semantic::ungroup(syntax, segment.value)).value;
    if (const auto *string = std::get_if<ast::StringLiteral>(&value)) {
        if (string->value.empty()) {
            values.insert(values.end(), {descriptor(state, options, true), size, integer(state, 0)});
        }
        for (const auto character : string->value) {
            values.insert(values.end(), {metadata, size, integer(state, character)});
        }
    } else {
        values.insert(values.end(), {metadata, size, state.values.at(&syntax.expression(segment.value))});
    }
}

// OTP coerces integer literals to binary64 before comparing an extracted float, without width rounding.
llvm::Value *expected_float(ExpressionLowering &state, const semantic::MatchLiteral &literal) {
    if (const auto *real = std::get_if<ast::FloatLiteral>(&literal)) {
        return lower_float(state, real->value);
    }
    auto *value = lower_integer(state, std::get<ast::IntegerLiteral>(literal).value.decimal);
    return lower_operation(state, abi::v1::ImmediateOperation::to_float, value);
}

// Compare the finite extracted value with the original literal after OTP's integer-to-float coercion.
void literal_constraint(ExpressionLowering &state, llvm::Value *actual, llvm::Value *expected,
                        llvm::BasicBlock *mismatch) {
    auto *success = llvm::BasicBlock::Create(state.entry.getContext(), "binary.literal", &state.entry);
    state.builder.CreateCondBr(lower_exact(state, actual, expected), success, mismatch);
    state.builder.SetInsertPoint(success);
}
} // namespace

llvm::Value *lower_bits_operation(ExpressionLowering &state, const Op operation,
                                  const std::span<llvm::Value *const> values) {
    return service(state, operation, values).value;
}

llvm::Value *lower_bits(ExpressionLowering &state, const ast::Bitstring &binary) {
    std::vector<llvm::Value *> values;
    for (const auto &segment : binary.segments) {
        segment_values(state, segment, values);
    }
    return service(state, Op::make, values).value;
}

BitLowering lower_bit_pattern(ExpressionLowering &state, const semantic::MatchNode &node,
                              const std::span<llvm::Value *> values, llvm::BasicBlock *mismatch) {
    auto *saved = std::exchange(state.rejection, mismatch);
    std::vector<llvm::Value *> arguments{values[node.input]};
    auto operation = Op::test;
    llvm::Value *expected = nullptr;
    if (node.operation != semantic::MatchOperation::binary_start) {
        arguments.push_back(values[node.index]);
        operation = Op::finish;
    }
    if (node.operation == semantic::MatchOperation::binary_extract && node.bits) {
        operation = Op::extract;
        const auto options = *node.bits;
        auto *size = node.key ? lower_body(state, *node.key) : integer(state, options.size);
        if (node.literal) {
            expected = expected_float(state, *node.literal);
        }
        arguments.insert(arguments.end(), {descriptor(state, options), size, integer(state, 0)});
    }
    const auto result = service(state, operation, arguments);
    if (expected) {
        literal_constraint(state, result.value, expected, mismatch);
    }
    state.rejection = saved;
    return result;
}

llvm::Value *lower_binary_part(ExpressionLowering &state, const std::span<llvm::Value *const> values) {
    if (values.size() == 3) {
        return service(state, Op::part, values).value;
    }
    auto *tuple = values[1];
    auto *rejection = bad_argument_exit(state);
    lower_inspection(state, abi::v1::ContainerInspection::tuple_shape, tuple, 2, rejection);
    auto *start = lower_inspection(state, abi::v1::ContainerInspection::tuple_element, tuple, 0, rejection);
    auto *length = lower_inspection(state, abi::v1::ContainerInspection::tuple_element, tuple, 1, rejection);
    return service(state, Op::part, std::array{values[0], start, length}).value;
}
} // namespace erlang_aot::codegen

#include "../semantic/capabilities.hpp"
#include "../semantic/funs.hpp"
#include "../semantic/symbols.hpp"
#include "lowering_state.hpp"
#include "runtime_symbols.hpp"
#include <algorithm>
#include <erlang_aot/abi/equality.hpp>
#include <erlang_aot/abi/frames.hpp>
#include <erlang_aot/abi/term.hpp>
#include <llvm/IR/Module.h>
#include <optional>
#include <stdexcept>
#include <variant>

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

// The evaluated value of an operand expression.
llvm::Value *value_of(ExpressionLowering &state, const ast::ExprId &id) {
    return state.values.at(&state.module.syntax->expression(id));
}

// A word array of `count` words (at least one) that lower_frames makes the process registers.
llvm::Value *registers(ExpressionLowering &state, std::size_t count) {
    auto *array = state.builder.CreateAlloca(
        state.word, llvm::ConstantInt::get(state.word, std::max<std::size_t>(count, 1)), "apply.arguments");
    array->setAlignment(llvm::Align(state.word->getBitWidth() / 8));
    return array;
}

// A register array holding the call's arguments; a fun's captured values follow them.
llvm::Value *arguments(ExpressionLowering &state, const ast::CallExpression &call) {
    auto &builder = state.builder;
    auto *array = registers(state, call.arguments.size());
    const llvm::Align alignment(state.word->getBitWidth() / 8);
    for (std::size_t i = 0; i < call.arguments.size(); ++i) {
        builder.CreateAlignedStore(value_of(state, call.arguments[i]),
                                   builder.CreateGEP(state.word, array, llvm::ConstantInt::get(state.word, i)),
                                   alignment);
    }
    return array;
}

// Call a preparation service returning the FrameDescriptor to enter (null after it raised), then transfer there with
// the registers in `array`: a tail transfer in tail position.
llvm::Value *transfer(ExpressionLowering &state, const ast::Expression &expression, llvm::FunctionCallee service,
                      llvm::ArrayRef<llvm::Value *> operands, llvm::Value *array) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *ptr = builder.getPtrTy();
    auto *frame = builder.CreateCall(service, operands, "apply.frame");
    propagate_failure(state);
    auto marker = output.getOrInsertFunction(APPLY_MARKER, llvm::FunctionType::get(state.word, {ptr, ptr, ptr}, false));
    auto *result = builder.CreateCall(marker, {state.entry.getArg(0), array, frame}, "apply.result");
    if (state.tail_calls && state.tail_calls->contains(&expression)) {
        // The called function's result is this function's: lower_frames turns the return into a tail transfer.
        builder.CreateRet(result);
        builder.SetInsertPoint(llvm::BasicBlock::Create(state.entry.getContext(), "tail.dead", &state.entry));
        return llvm::ConstantInt::get(state.word, abi::v1::empty_list);
    }
    propagate_failure(state);
    return result;
}

// Declare a dynamic call service returning a FrameDescriptor: the context, `words` term words, then the register
// array when `registers` is set.
template <typename Service>
llvm::FunctionCallee frame_service(ExpressionLowering &state, std::size_t words, bool registers) {
    auto &output = *state.entry.getParent();
    auto *ptr = state.builder.getPtrTy();
    std::vector<llvm::Type *> parameters{ptr};
    parameters.insert(parameters.end(), words, state.word);
    if (registers) {
        parameters.push_back(ptr);
    }
    return output.getOrInsertFunction(services::symbol<Service>(output.getTargetTriple()),
                                      llvm::FunctionType::get(ptr, parameters, false));
}

// The value a variable part of fun M:F/A reads; binding analysis resolved every one.
llvm::Value *read_value(const ExpressionLowering &state, const std::optional<semantic::BindingId> &read) {
    if (!read) {
        throw std::invalid_argument("lowering: unresolved fun operand");
    }
    return state.bindings.at(*read);
}

// One part of fun M:F/A: its atom, or the value of its variable.
llvm::Value *name_operand(ExpressionLowering &state, const std::variant<ast::Atom, ast::Variable> &part,
                          const std::optional<semantic::BindingId> &read) {
    const auto *atom = std::get_if<ast::Atom>(&part);
    return atom ? lower_atom(state, *atom) : read_value(state, read);
}

// The arity of fun M:F/A: a small integer (capability analysis admitted 0..255), or the value of its variable.
llvm::Value *arity_operand(ExpressionLowering &state, const std::variant<Integer, ast::Variable> &part,
                           const std::optional<semantic::BindingId> &read) {
    const auto *number = std::get_if<Integer>(&part);
    const auto count = number ? semantic::arity(*number) : std::nullopt;
    if (!count) {
        return read_value(state, read);
    }
    const auto value = static_cast<std::int64_t>(*count);
    return llvm::ConstantInt::get(state.word, state.word->getBitWidth() == 32
                                                  ? *abi::v1::IntegerEncoding<32>::encode(value)
                                                  : *abi::v1::IntegerEncoding<64>::encode(value));
}

// Build fun M:F/A with variables through erlang_aot_make_external_fun_v1; badarg raises.
llvm::Value *dynamic_fun(ExpressionLowering &state, const ast::Expression &expression,
                         const ast::RemoteFunReference &reference) {
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    const auto &reads = state.function.fun_operands.at(&expression);
    auto *module = name_operand(state, reference.module, reads[0]);
    auto *function = name_operand(state, reference.name, reads[1]);
    auto *arity = arity_operand(state, reference.arity, reads[2]);
    auto *ptr = builder.getPtrTy();
    auto service = output.getOrInsertFunction(
        services::symbol<services::MakeExternalFun>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(), {ptr, state.word, state.word, state.word, ptr}, false));
    auto *slot = root_slot(state);
    builder.CreateCall(service, {state.entry.getArg(0), module, function, arity, slot});
    propagate_failure(state);
    return builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8), "fun.value");
}
} // namespace

llvm::Value *lower_fun(ExpressionLowering &state, const ast::Expression &expression) {
    if (const auto *reference = std::get_if<ast::RemoteFunReference>(&expression.value);
        reference && semantic::dynamic_fun(*reference)) {
        return dynamic_fun(state, expression, *reference);
    }
    auto &builder = state.builder;
    auto &output = *state.entry.getParent();
    auto *ptr = builder.getPtrTy();
    auto service = output.getOrInsertFunction(
        services::symbol<services::MakeFun>(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(), {ptr, ptr, ptr, state.word, ptr}, false));
    // An anonymous fun's captured values go to consecutive rooted slots.
    const auto &captures = state.module.funs.at(state.module.fun_entries.at(&expression)).captures;
    llvm::Value *values = llvm::ConstantPointerNull::get(ptr);
    if (!captures.empty()) {
        values = builder.CreateGEP(state.word, state.roots->buffer,
                                   llvm::ConstantInt::get(state.word, state.roots->next), "fun.captures");
        for (const auto &identity : captures) {
            root_value(state, state.bindings.at(identity));
        }
    }
    auto *slot = root_slot(state);
    builder.CreateCall(service, {state.entry.getArg(0), descriptor(state, expression), values,
                                 llvm::ConstantInt::get(state.word, captures.size()), slot});
    propagate_failure(state);
    return builder.CreateAlignedLoad(state.word, slot, llvm::Align(state.word->getBitWidth() / 8), "fun.value");
}

llvm::Value *lower_fun_call(ExpressionLowering &state, const ast::Expression &expression,
                            const ast::CallExpression &call) {
    auto *fun = value_of(state, call.target);
    auto *array = arguments(state, call);
    auto *count = llvm::ConstantInt::get(state.word, call.arguments.size());
    return transfer(state, expression, frame_service<services::Apply>(state, 2, true),
                    {state.entry.getArg(0), fun, count, array}, array);
}

llvm::Value *lower_dynamic_call(ExpressionLowering &state, const ast::Expression &expression,
                                const ast::CallExpression &call) {
    const auto &syntax = *state.module.syntax;
    const auto &remote =
        std::get<ast::RemoteExpression>(syntax.expression(semantic::ungroup(syntax, call.target)).value);
    auto *module = value_of(state, remote.module);
    auto *function = value_of(state, remote.function);
    auto *array = arguments(state, call);
    auto *count = llvm::ConstantInt::get(state.word, call.arguments.size());
    return transfer(state, expression, frame_service<services::Call>(state, 3, false),
                    {state.entry.getArg(0), module, function, count}, array);
}

llvm::Value *lower_apply(ExpressionLowering &state, const ast::Expression &expression,
                         const ast::CallExpression &call) {
    auto *array = registers(state, abi::v1::register_count);
    std::vector<llvm::Value *> operands{state.entry.getArg(0)};
    for (const auto &argument : call.arguments) {
        operands.push_back(value_of(state, argument));
    }
    operands.push_back(array);
    const auto service = call.arguments.size() == 2 ? frame_service<services::ApplyList>(state, 2, true)
                                                    : frame_service<services::CallList>(state, 3, true);
    return transfer(state, expression, service, operands, array);
}
} // namespace erlang_aot::codegen

#include "lowering_state.hpp"
#include <array>
#include <bit>
#include <llvm/IR/Module.h>
#include <llvm/TargetParser/Triple.h>

namespace erlang_aot::codegen {
namespace {
// Select native C++ service linkage using target platform and width.
std::string_view symbol(const llvm::Triple &triple) {
    if (triple.isWindowsMSVCEnvironment()) {
        return triple.isArch64Bit() ? "?erlang_aot_float_v1@@YAEPEAXPEBD_KPEA_K@Z"
                                    : "?erlang_aot_float_v1@@YAEPAXPBDIPAI@Z";
    }
    return triple.isArch64Bit() ? "_Z19erlang_aot_float_v1PvPKcmPm" : "_Z19erlang_aot_float_v1PvPKcjPj";
}
} // namespace

llvm::Value *lower_float(ExpressionLowering &state, double value) {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    std::array<char, 8> encoded{};
    for (std::size_t i = 0; i < encoded.size(); ++i) {
        encoded[i] = static_cast<char>((bits >> ((7 - i) * 8)) & 255);
    }
    const std::string_view payload(encoded.data(), encoded.size());
    auto &output = *state.entry.getParent();
    auto &builder = state.builder;
    auto *bytes = llvm::ConstantDataArray::getString(output.getContext(), payload, false);
    auto *text = new llvm::GlobalVariable(output, bytes->getType(), true, llvm::GlobalValue::PrivateLinkage, bytes,
                                          "float.literal");
    auto *slot = root_slot(state);
    auto service = output.getOrInsertFunction(
        symbol(output.getTargetTriple()),
        llvm::FunctionType::get(builder.getInt8Ty(),
                                {builder.getPtrTy(), builder.getPtrTy(), state.word, builder.getPtrTy()}, false));
    auto *outcome = builder.CreateCall(
        service, {state.entry.getArg(0), text, llvm::ConstantInt::get(state.word, payload.size()), slot},
        "float.outcome");
    return checked_value(state, {outcome, slot}, bad_arithmetic_exit(state));
}

} // namespace erlang_aot::codegen

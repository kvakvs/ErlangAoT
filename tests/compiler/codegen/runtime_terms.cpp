#include "codegen/compilation.hpp"
#include "codegen/target.hpp"
#include "codegen/term_abi.hpp"
#include <erlang_aot/abi/term.hpp>
#include <erlang_aot/runtime/terms.hpp>
#include <iostream>
#include <llvm/IR/Constants.h>
#include <stdexcept>

namespace {
// Fail visibly in every build configuration, independently of NDEBUG.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Construct constants independently with LLVM's signed shifts, then cross the runtime word boundary.
void check_constants() {
    using Encoding = erlang_aot::abi::v1::NativeIntegerEncoding;
    namespace runtime = erlang_aot::runtime;
    namespace codegen = erlang_aot::codegen;
    codegen::CompilationRequest request;
    request.inputs.emplace_back("runtime-terms.erl", erlang_aot::ast::Module{});
    codegen::Compilation compilation(std::move(request));
    require(codegen::configure_target(compilation), "native target setup failed");
    auto *type = codegen::term_type(compilation);
    require(type != nullptr, "native term type unavailable");
    require(type->getBitWidth() == sizeof(runtime::Word) * 8, "native runtime/LLVM width mismatch");
    for (const auto value : {Encoding::minimum, std::int64_t{-42}, std::int64_t{-1}, std::int64_t{0}, std::int64_t{42},
                             Encoding::maximum}) {
        auto bits = llvm::APInt(type->getBitWidth(), static_cast<std::uint64_t>(value), true).shl(4);
        bits |= 0xf;
        const auto *constant = llvm::ConstantInt::get(type, bits);
        const auto word = static_cast<runtime::Word>(llvm::cast<llvm::ConstantInt>(constant)->getZExtValue());
        require(runtime::classify_immediate(word) == runtime::TermKind::smallint, "LLVM tag mismatch");
        require(runtime::decode_integer(word) == value, "runtime cannot decode LLVM integer");
        require(runtime::encode_integer(value) == word, "runtime/LLVM encoded constants differ");
    }
}
} // namespace

// LLVM is a test-only producer; erlang_runtime itself remains independently linkable without the SDK.
int main() {
    try {
        check_constants();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

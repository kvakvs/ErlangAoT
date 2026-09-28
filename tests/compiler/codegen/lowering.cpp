#include "codegen/lowering.hpp"
#include "codegen/emission.hpp"
#include "codegen/llvm_state.hpp"
#include "semantic/bindings.hpp"
#include "semantic/capabilities.hpp"
#include "semantic/types/contracts.hpp"
#include <cstdio>
#include <erlang_aot/compiler/parser.hpp>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <stdexcept>

using namespace erlang_aot;
namespace cg = erlang_aot::codegen;

// Exercise real source until the artifact-producing CLI replaces this stage adapter.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Own parsed fixture syntax before borrowing any semantic declaration addresses.
cg::Compilation fixture(const char *name, const std::string &triple = {}) {
    SourceManager sources;
    const auto path = std::filesystem::path(LOWERING_FIXTURES) / name;
    PreprocessorSession pp(sources.read(path));
    auto parsed = parse_module(pp);
    require(!parsed.failed, "fixture parse failed");
    cg::CompilationRequest request;
    request.target_triple = triple;
    request.inputs.emplace_back(path, std::move(parsed.module));
    return cg::Compilation(std::move(request));
}

// Run existing declaration, capability, binding, call and type stages without replacement mocks.
bool analyze_and_lower(cg::Compilation &compilation) {
    const semantic::Reporter report = [](const Diagnostic &diagnostic) {
        require(diagnostic.severity != Severity::error, "fixture semantics failed");
    };
    std::vector<std::unique_ptr<semantic::Module>> modules;
    for (const auto &input : compilation.request().inputs) {
        auto module = semantic::index(input.syntax, "fixture.erl", report);
        semantic::check_capabilities(*module, report, 64);
        semantic::bind_parameters(*module, report);
        modules.push_back(std::move(module));
    }
    const auto calls = semantic::resolve_calls(modules, report);
    const auto declared = semantic::types::resolve_declarations(modules, report);
    const auto inferred = semantic::types::infer(calls);
    semantic::types::check_contracts(*declared, *inferred, calls, report);
    return cg::lower(compilation, modules, *inferred);
}

// Verify emitted bytes are a native object containing executable code.
void inspect_object(cg::Compilation &compilation) {
    require(cg::emit_objects(compilation), "real-source object emission failed");
    const auto &output = compilation.result().outputs().front();
    const llvm::StringRef bytes(reinterpret_cast<const char *>(output.bytes.data()), output.bytes.size());
    auto object = llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(bytes, "fixture"));
    if (!object) {
        throw std::runtime_error(llvm::toString(object.takeError()));
    }
    bool executable = false;
    for (const auto &section : (*object)->sections()) {
        executable |= section.isText() && section.getSize() != 0;
    }
    require(executable, "real-source object has no code");
}

// Compare decoded returns, including both representable endpoints, independently of inferred types.
void constants(const char *name, const std::string &triple, unsigned bits) {
    auto compilation = fixture(name, triple);
    require(analyze_and_lower(compilation), "constant lowering failed");
    const auto minimum = -(std::int64_t{1} << (bits - 5));
    const std::vector<std::int64_t> expected{42, -42, 0, minimum, -minimum - 1};
    std::size_t index = 0;
    for (const auto &function : *cg::detail::state(compilation).modules.front()) {
        const auto *ret = llvm::cast<llvm::ReturnInst>(function.getEntryBlock().getTerminator());
        const auto &value = llvm::cast<llvm::ConstantInt>(ret->getReturnValue())->getValue();
        require(value.getBitWidth() == bits, "literal uses host width");
        require(value.ashr(4).getSExtValue() == expected.at(index++), "literal was truncated or mistagged");
        require(function.getCallingConv() == llvm::CallingConv::C && function.arg_size() == 2, "wrong entry ABI");
    }
    require(index == expected.size(), "missing constant definition");
    inspect_object(compilation);
}

// A host-valid literal must still be rejected when compiling for a narrower target.
void target_overflow() {
    auto compilation = fixture("constants64.erl", "i686-unknown-linux-gnu");
    require(!analyze_and_lower(compilation), "target overflow accepted");
    require(compilation.result().status() == cg::CompilationStatus::failed, "lowering did not latch failure");
    require(!cg::emit_objects(compilation) && compilation.result().outputs().empty(), "failed batch emitted output");
}

// Project source positions directly; equal argument values cannot collapse distinct slots.
void parameters(const std::string &triple = {}, unsigned bits = sizeof(void *) * 8) {
    auto compilation = fixture("parameters.erl", triple);
    require(analyze_and_lower(compilation), "parameter lowering failed");
    const std::vector<std::uint64_t> expected{0, 0, 1, 2};
    std::size_t index = 0;
    for (const auto &function : *cg::detail::state(compilation).modules.front()) {
        auto instruction = function.getEntryBlock().begin();
        const auto *slot = llvm::cast<llvm::GetElementPtrInst>(&*instruction++);
        const auto *load = llvm::cast<llvm::LoadInst>(&*instruction++);
        const auto *ret = llvm::cast<llvm::ReturnInst>(&*instruction);
        require(slot->hasOneUse() && *slot->user_begin() == load && ret->getReturnValue() == load,
                "projection changed the term");
        require(slot->getPointerOperand() == function.getArg(1), "parameter lost argument array");
        require(!slot->isInBounds(), "parameter adds an unjustified pointer promise");
        const auto offset = llvm::cast<llvm::ConstantInt>(slot->getOperand(1))->getZExtValue();
        require(offset == expected.at(index++), "parameter order changed");
        require(load->getAlign().value() == bits / 8, "parameter alignment differs from ABI");
    }
    require(index == expected.size(), "missing projection");
    inspect_object(compilation);
}

// Check source-to-object behavior without publishing artifacts or linking a program.
int main() {
    try {
        constants(sizeof(void *) == 8 ? "constants64.erl" : "constants32.erl", "", sizeof(void *) * 8);
#ifdef ERLANG_AOT_LLVM_X86
        constants("constants32.erl", "i686-unknown-linux-gnu", 32);
        constants("constants64.erl", "x86_64-unknown-linux-gnu", 64);
        target_overflow();
        parameters("i686-unknown-linux-gnu", 32);
#endif
        parameters();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected lowering failure\n", stderr);
        return 1;
    }
}

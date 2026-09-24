#include "codegen/emission.hpp"
#include "codegen/llvm_state.hpp"
#include "codegen/target.hpp"
#include "codegen/verification.hpp"
#include <fstream>
#include <iostream>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <stdexcept>

using namespace erlang_aot;
using namespace erlang_aot::codegen;

// Keep assertions active in release builds as well as debug builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Construct two independently emitted synthetic modules with a known external symbol.
Compilation synthetic_batch(const std::string &triple = {}) {
    CompilationRequest request;
    request.target_triple = triple;
    request.inputs.emplace_back("first.erl", ast::Module{});
    request.inputs.emplace_back("second.erl", ast::Module{});
    Compilation compilation(std::move(request));
    require(configure_target(compilation), "target setup failed");
    for (auto &module : detail::state(compilation).modules) {
        llvm::IRBuilder<> builder(module->getContext());
        auto *type = llvm::FunctionType::get(builder.getInt32Ty(), false);
        auto *function = llvm::Function::Create(type, llvm::GlobalValue::ExternalLinkage, "answer", *module);
        builder.SetInsertPoint(llvm::BasicBlock::Create(module->getContext(), "entry", function));
        builder.CreateRet(builder.getInt32(42));
    }
    return compilation;
}

// Inspect actual object headers, executable sections and the external symbol through LLVM's object reader.
void inspect(const OutputBuffer &output, llvm::Triple::ArchType architecture) {
    require(output.kind == OutputKind::object && !output.bytes.empty(), "missing object buffer");
    const auto bytes = llvm::StringRef(reinterpret_cast<const char *>(output.bytes.data()), output.bytes.size());
    auto object = llvm::object::ObjectFile::createObjectFile(llvm::MemoryBufferRef(bytes, output.module_name));
    if (!object) {
        throw std::runtime_error(llvm::toString(object.takeError()));
    }
    require((*object)->getArch() == architecture, "wrong object architecture");
    bool text = false;
    for (const auto &section : (*object)->sections()) {
        text |= section.isText() && section.getSize() != 0;
    }
    require(text, "object has no executable section");
    bool answer = false;
    for (const auto &symbol : (*object)->symbols()) {
        auto name = symbol.getName();
        if (!name) {
            throw std::runtime_error(llvm::toString(name.takeError()));
        }
        answer |= *name == "answer" || *name == "_answer";
    }
    require(answer, "object lost known symbol");
}

// Repeated emission replaces bytes without mutating source IR and survives compilation teardown.
CompilationResult check_valid(const std::string &triple = {}) {
    auto compilation = synthetic_batch(triple);
    const auto arch = detail::state(compilation).target_machine->getTargetTriple().getArch();
    require(emit_objects(compilation), "object emission failed");
    const auto first = compilation.result().outputs().front().bytes;
    require(emit_objects(compilation) && verify_ir(compilation), "repeat emission failed");
    require(compilation.result().outputs().size() == 2, "repeat emission duplicated outputs");
    require(first == compilation.result().outputs().front().bytes, "repeat emission changed bytes");
    for (const auto &diagnostic : compilation.result().diagnostics()) {
        require(diagnostic.level == DiagnosticLevel::note, "successful emission produced a warning/error");
    }
    require(compilation.result().complete(), "emitted batch could not complete");
    auto result = std::move(compilation).take_result();
    for (const auto &output : result.outputs()) {
        inspect(output, arch);
    }
    return result;
}

// Mutation after successful verification/emission must invalidate all previously staged objects.
void check_reverification() {
    auto compilation = synthetic_batch();
    require(emit_objects(compilation), "baseline emission failed");
    const auto previous = compilation.result().diagnostics().size();
    auto &module = *detail::state(compilation).modules.back();
    module.getFunction("answer")->getEntryBlock().getTerminator()->eraseFromParent();
    require(!emit_objects(compilation), "emission reused stale verification");
    require(compilation.result().outputs().empty(), "failed batch retained earlier objects");
    require(compilation.result().diagnostics().size() == previous + 1, "missing verification error");
    require(!emit_objects(compilation), "failed emission retried");
    require(compilation.result().diagnostics().size() == previous + 1, "duplicate failure diagnostic");
}

// Valid IR with a failing assembler directive reaches the emission diagnostic callback.
void check_emission_error() {
    auto compilation = synthetic_batch();
    detail::state(compilation)
        .modules.back()
        ->setModuleInlineAsm(llvm::Module::GlobalAsmFragment(std::string(".error \"intentional emission failure\"\n")));
    require(verify_ir(compilation), "assembly fixture failed IR verification");
    require(!emit_objects(compilation), "assembler error was accepted");
    require(compilation.result().outputs().empty(), "assembler error retained partial batch");
    require(compilation.result().status() == CompilationStatus::failed, "assembler error did not latch failure");
    require(!compilation.result().diagnostics().empty(), "assembler error lost its diagnostic");
}

// Optionally write a test-owned object for external llvm-readobj/llvm-nm inspection.
void save(const CompilationResult &result, const char *path) {
    const auto outputs = result.outputs();
    const auto &bytes = outputs.front().bytes;
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    file.close();
    require(!file.fail(), "cannot save inspection fixture");
}

// Exercise native emission, available cross backends and both verification and code-generation failures.
int main(int argc, char **argv) {
    try {
        const auto native = check_valid();
        if (argc == 2) {
            save(native, argv[1]);
        }
#ifdef ERLANG_AOT_LLVM_X86
        check_valid("i686-unknown-linux-gnu");
        check_valid("x86_64-pc-windows-msvc");
#endif
#ifdef ERLANG_AOT_LLVM_ARM
        check_valid("armv7-unknown-linux-gnueabihf");
#endif
#ifdef ERLANG_AOT_LLVM_AArch64
        check_valid("aarch64-unknown-linux-gnu");
#endif
        check_reverification();
        check_emission_error();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

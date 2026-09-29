#include "codegen/serialization.hpp"
#include "codegen/optimization.hpp"
#include "lowering_support.hpp"
#include <llvm/AsmParser/Parser.h>
#include <llvm/Bitcode/BitcodeReader.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/SourceMgr.h>

// Reparse with SDK readers and inspect public entries rather than snapshotting LLVM formatting.
void round_trip(const cg::OutputBuffer &output) {
    llvm::LLVMContext context;
    const llvm::StringRef bytes(reinterpret_cast<const char *>(output.bytes.data()), output.bytes.size());
    std::unique_ptr<llvm::Module> module;
    if (output.kind == cg::OutputKind::llvm_ir) {
        llvm::SMDiagnostic diagnostic;
        const auto text = bytes.str();
        module = llvm::parseAssemblyString(text, diagnostic, context);
        if (!module) {
            diagnostic.print("serialization", llvm::errs());
        }
    } else {
        auto parsed = llvm::parseBitcodeFile(llvm::MemoryBufferRef(bytes, output.module_name), context);
        if (!parsed) {
            throw std::runtime_error(llvm::toString(parsed.takeError()));
        }
        module = std::move(*parsed);
    }
    require(module && !llvm::verifyModule(*module, &llvm::errs()), "serialized module did not round trip");
    const auto symbol = semantic::encode_symbol({"answer", "identity", 1});
    const auto *identity = module->getFunction(symbol);
    require(identity && !identity->isDeclaration() && identity->hasExternalLinkage(), "lost public identity");
    require(identity->arg_size() == 2 && identity->getReturnType()->isIntegerTy(), "changed generic ABI");
    require(module->getFunction(semantic::encode_symbol({"answer", "", 0}) + ".register"), "lost registration");
}

// Serialization is deterministic, reusable for snapshots and gated by fresh verification.
void check(cg::OptimizationLevel level) {
    auto compilation = fixtures({"answer.erl"}, {}, level);
    require(analyze_and_lower(compilation), "lowering failed");
    const auto before = cg::snapshot_ir(compilation);
    require(before && compilation.result().outputs().empty(), "snapshot changed staged output");
    require(cg::optimize(compilation), "optimization failed");
    for (const auto kind : {cg::OutputKind::llvm_ir, cg::OutputKind::llvm_bitcode}) {
        require(cg::emit_ir(compilation, kind), "serialization failed");
        const auto bytes = compilation.result().outputs().front().bytes;
        require(cg::emit_ir(compilation, kind), "repeated serialization failed");
        require(compilation.result().outputs().size() == 1, "serialization duplicated artifacts");
        require(compilation.result().outputs().front().bytes == bytes, "serialization was nondeterministic");
        round_trip(compilation.result().outputs().front());
    }
    round_trip(before->front());
    auto &module = *cg::detail::state(compilation).modules.front();
    auto *function = module.getFunction(semantic::encode_symbol({"answer", "identity", 1}));
    function->getEntryBlock().getTerminator()->eraseFromParent();
    require(!cg::snapshot_ir(compilation), "invalid IR produced a snapshot");
    require(compilation.result().outputs().empty(), "invalid IR retained artifacts");
}

// Keep source-driven round trips independent of frontend product inspection switches.
int main() {
    try {
        check(cg::OptimizationLevel::none);
        check(cg::OptimizationLevel::speed);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

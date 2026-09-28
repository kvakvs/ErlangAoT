#include "lowering.hpp"
#include "llvm_state.hpp"
#include "lowering_expressions.hpp"
#include "target.hpp"
#include "term_abi.hpp"
#include "verification.hpp"
#include <llvm/IR/IRBuilder.h>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Reject malformed phase inputs before generating a body or narrowing a literal.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::invalid_argument(message);
    }
}

// Declare every definition first, retaining private linkage for unexported entries.
void declare(llvm::Module &output, const semantic::Module &module, llvm::FunctionType *signature,
             const semantic::types::Inference &inferred) {
    for (const auto &function : module.functions) {
        const auto &summary = inferred.functions.at(&function);
        require(summary.inputs.size() == function.key.arity, "lowering: inconsistent inferred arity");
        // Validate fact ownership, but never turn a source contract into an LLVM assumption.
        (void)inferred.graph.get(summary.result.type);
        require(output.getFunction(function.symbol) == nullptr, "lowering: duplicate definition");
        const auto linkage =
            function.exported ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::InternalLinkage;
        auto *entry = llvm::Function::Create(signature, linkage, function.symbol, output);
        entry->setCallingConv(llvm::CallingConv::C);
        entry->getArg(0)->setName("context");
        entry->getArg(1)->setName("arguments");
    }
}

// Keep generic bodies independent of declared types and inferred representation guesses.
void define(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word) {
    for (const auto &function : module.functions) {
        auto *entry = output.getFunction(function.symbol);
        const auto &syntax = *module.syntax;
        const auto &definition = std::get<ast::Function>(syntax.form(function.form).value);
        const auto root = definition.clauses.at(0).body.at(0);
        llvm::IRBuilder<> builder(llvm::BasicBlock::Create(output.getContext(), "entry", entry));
        builder.CreateRet(lower_expression(builder, *entry, module, function, root, word));
    }
}

// Enforce the one-to-one syntax/module ownership contract before creating declarations.
void validate_inputs(const Compilation &compilation, std::span<const std::unique_ptr<semantic::Module>> modules) {
    const auto &inputs = compilation.request().inputs;
    require(inputs.size() == modules.size(), "lowering: batch size mismatch");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        require(modules[i] && modules[i]->syntax == &inputs[i].syntax, "lowering: foreign syntax owner");
    }
}
} // namespace

bool lower(Compilation &compilation, std::span<const std::unique_ptr<semantic::Module>> modules,
           const semantic::types::Inference &inferred) {
    if (!configure_target(compilation)) {
        return false;
    }
    auto *signature = generated_function_type(compilation);
    if (!signature) {
        return false;
    }
    try {
        validate_inputs(compilation, modules);
        auto &outputs = detail::state(compilation).modules;
        auto *word = llvm::cast<llvm::IntegerType>(signature->getReturnType());
        for (std::size_t i = 0; i < modules.size(); ++i) {
            declare(*outputs[i], *modules[i], signature, inferred);
        }
        for (std::size_t i = 0; i < modules.size(); ++i) {
            define(*outputs[i], *modules[i], word);
        }
        return verify_ir(compilation);
    } catch (const std::exception &error) {
        compilation.result().report(
            {.level = DiagnosticLevel::error, .message = error.what(), .location = {}, .module_name = {}});
        return false;
    }
}
} // namespace erlang_aot::codegen

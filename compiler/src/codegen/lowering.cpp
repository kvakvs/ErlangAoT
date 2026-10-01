#include "lowering.hpp"
#include "llvm_state.hpp"
#include "lowering_expressions.hpp"
#include "module_registration.hpp"
#include "progress.hpp"
#include "source_locations.hpp"
#include "specialization_analysis.hpp"
#include "specialization_lowering.hpp"
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
    output.setModuleIdentifier(utf8(module.name));
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
void define(llvm::Module &output, const semantic::Module &module, llvm::IntegerType *word,
            const semantic::types::Inference &inferred) {
    for (const auto &function : module.functions) {
        auto *entry = output.getFunction(function.symbol);
        const auto &syntax = *module.syntax;
        const auto &definition = std::get<ast::Function>(syntax.form(function.form).value);
        const auto root = definition.clauses.at(0).body.at(0);
        llvm::IRBuilder<> builder(llvm::BasicBlock::Create(output.getContext(), "entry", entry));
        auto *result = lower_expression(builder, *entry, module, function, root, word, inferred);
        locate_source(builder, syntax, syntax.expression(root).source);
        builder.CreateRet(result);
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
            progress(compilation.request(), "lowering", compilation.request().inputs[i].source_path,
                     utf8(modules[i]->name));
            declare(*outputs[i], *modules[i], signature, inferred);
            if (compilation.request().annotate_source) {
                prepare_source_locations(*outputs[i], *modules[i],
                                         compilation.request().optimization == OptimizationLevel::speed,
                                         detail::state(compilation).source_scopes);
            }
        }
        for (std::size_t i = 0; i < modules.size(); ++i) {
            emit_registration(*outputs[i], *modules[i], word);
            define(*outputs[i], *modules[i], word, inferred);
        }
        progress_modules(compilation, "specialization");
        detail::state(compilation).specializations = analyze_specializations(compilation, modules, inferred);
        for (auto &output : outputs) {
            lower_specializations(*output, detail::state(compilation).specializations);
        }
        const auto &plan = detail::state(compilation).specializations;
        progress_modules(compilation, "specialization",
                         "batch installed=" + std::to_string(plan.lowered_variants) +
                             " skipped-measured-growth=" + std::to_string(plan.rejected_variants));
        return verify_ir(compilation);
    } catch (const std::exception &error) {
        compilation.result().report(
            {.level = DiagnosticLevel::error, .message = error.what(), .location = {}, .module_name = {}});
        return false;
    }
}
} // namespace erlang_aot::codegen

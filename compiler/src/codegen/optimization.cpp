#include "optimization.hpp"
#include "llvm_state.hpp"
#include "progress.hpp"
#include "verification.hpp"
#include <exception>
#include <llvm/Passes/PassBuilder.h>

namespace erlang_aot::codegen {
namespace {
// Select the standard LLVM pipeline; like Clang -Os, size mode is O2 over definitions marked optsize.
llvm::ModulePassManager pipeline(llvm::PassBuilder &builder, llvm::Module &module, const OptimizationLevel level) {
    if (level == OptimizationLevel::none) {
        return builder.buildO0DefaultPipeline(llvm::OptimizationLevel::O0);
    }
    if (level == OptimizationLevel::size) {
        for (auto &function : module) {
            if (!function.isDeclaration()) {
                function.addFnAttr(llvm::Attribute::OptimizeForSize);
            }
        }
    }
    return builder.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
}

// Analysis managers are local to a module and destroyed in reverse dependency order.
void optimize_module(llvm::Module &module, llvm::TargetMachine &machine, const OptimizationLevel level) {
    llvm::LoopAnalysisManager loops;
    llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager call_graph;
    llvm::ModuleAnalysisManager modules;
    llvm::PassBuilder builder(&machine);
    builder.registerModuleAnalyses(modules);
    builder.registerCGSCCAnalyses(call_graph);
    builder.registerFunctionAnalyses(functions);
    builder.registerLoopAnalyses(loops);
    builder.crossRegisterProxies(loops, functions, call_graph, modules);
    pipeline(builder, module, level).run(module, modules);
}
} // namespace

bool optimize(Compilation &compilation) {
    if (!verify_ir(compilation)) {
        return false;
    }
    auto &state = detail::state(compilation);
    state.result.discard_outputs();
    std::size_t index = 0;
    for (const auto &module : state.modules) {
        progress_module(compilation, index++, "optimization");
        try {
            optimize_module(*module, *state.target_machine, state.request.optimization);
        } catch (const std::exception &error) {
            state.result.report({.level = DiagnosticLevel::error,
                                 .message = "LLVM optimization failed: " + std::string(error.what()),
                                 .location = {},
                                 .module_name = module->getModuleIdentifier()});
            return false;
        }
    }
    return verify_ir(compilation);
}
} // namespace erlang_aot::codegen

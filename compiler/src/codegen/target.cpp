#include "target.hpp"
#include "llvm_state.hpp"
#include <algorithm>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/TargetParser/Host.h>
#include <string>
#include <vector>

namespace erlang_aot::codegen {
namespace {
// Serialize explicit enabled/disabled host features in stable order for reproducible IR later.
std::string host_features() {
    std::vector<std::string> features;
    for (const auto &feature : llvm::sys::getHostCPUFeatures()) {
        features.push_back(std::string(feature.second ? "+" : "-") + feature.first().str());
    }
    std::ranges::sort(features);
    std::string joined;
    for (const auto &feature : features) {
        if (!joined.empty()) {
            joined += ',';
        }
        joined += feature;
    }
    return joined;
}

// Own lookup failures in the batch diagnostic sink; never substitute a different architecture.
const llvm::Target *find_target(const llvm::Triple &triple, CompilationResult &result) {
    if (triple.getArch() == llvm::Triple::UnknownArch) {
        result.report({.level = DiagnosticLevel::error,
                       .message = "unknown target architecture in triple: " + triple.str(),
                       .location = {},
                       .module_name = {}});
        return nullptr;
    }
    std::string error;
    const auto *target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) {
        result.report({.level = DiagnosticLevel::error,
                       .message = "target backend unavailable for " + triple.str() + ": " + error,
                       .location = {},
                       .module_name = {}});
    }
    return target;
}

// Prefer detected CPU capabilities only for the native triple; foreign targets use LLVM's generic baseline.
std::unique_ptr<llvm::TargetMachine> create_machine(const CompilationRequest &request, CompilationResult &result) {
    const llvm::Triple host(llvm::Triple::normalize(llvm::sys::getProcessTriple()));
    const llvm::Triple triple(request.target_triple.empty() ? host.str()
                                                            : llvm::Triple::normalize(request.target_triple));
    const auto *target = find_target(triple, result);
    if (!target) {
        return nullptr;
    }
    const bool native = triple == host;
    const auto cpu = native ? llvm::sys::getHostCPUName().str() : "generic";
    const auto features = native ? host_features() : std::string{};
    const auto level =
        request.optimization == OptimizationLevel::speed ? llvm::CodeGenOptLevel::Default : llvm::CodeGenOptLevel::None;
    // PIC supports later shared modules; Small is the baseline address-range contract on all configured targets.
    auto machine = std::unique_ptr<llvm::TargetMachine>(target->createTargetMachine(
        triple, cpu, features, llvm::TargetOptions{}, llvm::Reloc::PIC_, llvm::CodeModel::Small, level));
    if (!machine) {
        result.report({.level = DiagnosticLevel::error,
                       .message = "cannot construct target machine for " + triple.str(),
                       .location = {},
                       .module_name = {}});
    }
    return machine;
}
} // namespace

bool configure_target(Compilation &compilation) {
    auto &state = detail::state(compilation);
    if (state.result.status() != CompilationStatus::incomplete) {
        return false;
    }
    if (state.target_machine) {
        return true;
    }
    detail::initialize_target_backends();
    auto machine = create_machine(state.request, state.result);
    if (!machine) {
        return false;
    }
    const auto layout = machine->createDataLayout();
    for (const auto &module : state.modules) {
        module->setTargetTriple(machine->getTargetTriple());
        module->setDataLayout(layout);
    }
    state.target_machine = std::move(machine);
    return true;
}
} // namespace erlang_aot::codegen

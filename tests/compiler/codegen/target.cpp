#include "codegen/target.hpp"
#include "codegen/llvm_state.hpp"
#include <bit>
#include <climits>
#include <iostream>
#include <llvm/TargetParser/Host.h>
#include <stdexcept>
#include <string>
#include <utility>

using namespace erlang_aot;
using namespace erlang_aot::codegen;

// Keep target-contract checks active in release builds as well as debug builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Create two empty modules so every target setup exercises whole-batch propagation.
CompilationRequest request(std::string triple = {}) {
    CompilationRequest batch;
    batch.target_triple = std::move(triple);
    batch.inputs.emplace_back("first.erl", ast::Module{});
    batch.inputs.emplace_back("second.erl", ast::Module{});
    return batch;
}

// Check the target-derived layout, relocation contract and unchanged incomplete result.
void check_layout(const Compilation &compilation, unsigned bits) {
    const auto &state = detail::state(compilation);
    const auto &machine = *state.target_machine;
    require(machine.createDataLayout().getPointerSizeInBits() == bits, "wrong target pointer width");
    require(machine.getRelocationModel() == llvm::Reloc::PIC_, "unexpected relocation model");
    require(machine.getCodeModel() == llvm::CodeModel::Small, "unexpected code model");
    for (const auto &module : state.modules) {
        require(module->getTargetTriple() == machine.getTargetTriple(), "module target mismatch");
        require(module->getDataLayout() == machine.createDataLayout(), "module layout mismatch");
        require(module->empty(), "target setup unexpectedly lowered code");
    }
    require(compilation.result().status() == CompilationStatus::incomplete, "target setup completed compilation");
    require(compilation.result().diagnostics().empty() && compilation.result().outputs().empty(), "unexpected results");
}

// Native selection follows the running process, retains detected capabilities and survives moves.
void check_native() {
    Compilation compilation(request());
    require(configure_target(compilation), "native target setup failed");
    check_layout(compilation, sizeof(void *) * CHAR_BIT);
    const auto *machine = detail::state(compilation).target_machine.get();
    const auto layout = machine->createDataLayout();
    require(layout.getPointerABIAlignment(0).value() == alignof(void *), "native pointer alignment mismatch");
    require(layout.isLittleEndian() == (std::endian::native == std::endian::little), "native byte order mismatch");
    const llvm::Triple host(llvm::Triple::normalize(llvm::sys::getProcessTriple()));
    require(machine->getTargetTriple() == host, "default target is not the process host");
    require(machine->getTargetCPU() == llvm::sys::getHostCPUName(), "host CPU preference lost");
    require(machine->getOptLevel() == llvm::CodeGenOptLevel::None, "default machine optimization is not O0");
    const auto features = "," + machine->getTargetFeatureString().str() + ",";
    for (const auto &feature : llvm::sys::getHostCPUFeatures()) {
        const auto spelling = std::string(feature.second ? ",+" : ",-") + feature.first().str() + ",";
        require(features.contains(spelling), "detected host feature lost");
    }
    Compilation moved(std::move(compilation));
    require(configure_target(moved) && detail::state(moved).target_machine.get() == machine,
            "machine rebuilt after move");
    Compilation explicit_host(request(host.str()));
    require(configure_target(explicit_host), "explicit host setup failed");
    require(detail::state(explicit_host).target_machine->getTargetFeatureString() == machine->getTargetFeatureString(),
            "explicit host differs from native default");
    const auto result = std::move(moved).take_result();
    require(result.status() == CompilationStatus::incomplete, "target teardown changed status");
}

// Foreign targets retain their requested architecture/OS, baseline CPU and target-sized words.
void check_foreign(const std::string &triple, unsigned bits, llvm::Triple::ObjectFormatType format) {
    auto batch = request(triple);
    batch.optimization = OptimizationLevel::speed;
    Compilation compilation(std::move(batch));
    require(configure_target(compilation), "foreign target setup failed");
    check_layout(compilation, bits);
    const auto &machine = *detail::state(compilation).target_machine;
    require(machine.getTargetTriple() == llvm::Triple(llvm::Triple::normalize(triple)), "requested triple lost");
    const llvm::Triple host(llvm::Triple::normalize(llvm::sys::getProcessTriple()));
    if (machine.getTargetTriple() != host) {
        require(machine.getTargetCPU() == "generic" && machine.getTargetFeatureString().empty(),
                "host leaked into target");
    }
    require(machine.getTargetTriple().getObjectFormat() == format, "wrong target object format");
    require(machine.getOptLevel() == llvm::CodeGenOptLevel::Default, "speed policy lost");
}

// Distinguish invalid triples from valid architectures without a configured backend.
enum class TargetFailure : std::uint8_t { unknown_architecture, unavailable_backend };

// Check diagnostic classification as well as the absence of fallback state and output.
void check_failure(const std::string &triple, TargetFailure failure) {
    Compilation compilation(request(triple));
    require(compilation.result().add_output({"staged", OutputKind::object, {std::byte{1}}}), "staging failed");
    require(!configure_target(compilation), "invalid target accepted");
    require(!configure_target(compilation), "failed target retried successfully");
    const auto &state = detail::state(compilation);
    require(!state.target_machine, "failed target substituted a machine");
    for (const auto &module : state.modules) {
        require(module->getDataLayoutStr().empty() && module->getTargetTriple().str().empty(),
                "failure mutated module");
    }
    const auto result = std::move(compilation).take_result();
    require(result.status() == CompilationStatus::failed && result.outputs().empty(), "failure retained artifacts");
    require(result.diagnostics().size() == 1, "missing or repeated target diagnostic");
    const auto message =
        failure == TargetFailure::unknown_architecture ? "unknown target architecture" : "backend unavailable";
    require(result.diagnostics().front().message.contains(message), "wrong target diagnostic");
    require(result.diagnostics().front().message.contains(triple), "diagnostic lost target context");
}

// Exercise each configured backend without claiming foreign object emission or execution.
void check_cross_targets() {
#ifdef ERLANG_AOT_LLVM_X86
    check_foreign("i686-unknown-linux-gnu", 32, llvm::Triple::ELF);
    check_foreign("x86_64-unknown-linux-gnu", 64, llvm::Triple::ELF);
    check_foreign("x86_64-linux-gnu", 64, llvm::Triple::ELF);
    check_foreign("i686-pc-windows-msvc", 32, llvm::Triple::COFF);
    check_foreign("x86_64-pc-windows-msvc", 64, llvm::Triple::COFF);
#else
    check_failure("x86_64-unknown-linux-gnu", TargetFailure::unavailable_backend);
#endif
#ifdef ERLANG_AOT_LLVM_ARM
    check_foreign("armv7-unknown-linux-gnueabihf", 32, llvm::Triple::ELF);
#else
    check_failure("armv7-unknown-linux-gnueabihf", TargetFailure::unavailable_backend);
#endif
#ifdef ERLANG_AOT_LLVM_AArch64
    check_foreign("aarch64-unknown-linux-gnu", 64, llvm::Triple::ELF);
#else
    check_failure("aarch64-unknown-linux-gnu", TargetFailure::unavailable_backend);
#endif
}

// Report independent target scenarios without aborting on ordinary test failures.
int main() {
    try {
        check_native();
        check_failure("notacpu-unknown-linux-gnu", TargetFailure::unknown_architecture);
        check_failure("riscv64-unknown-linux-gnu", TargetFailure::unavailable_backend);
        check_cross_targets();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

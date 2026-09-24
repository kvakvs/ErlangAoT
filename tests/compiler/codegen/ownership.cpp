#include "codegen/llvm_state.hpp"
#include <erlang_aot/compiler/parser.hpp>
#include <iostream>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/DiagnosticInfo.h>
#include <llvm/IR/DiagnosticPrinter.h>
#include <new>
#include <stdexcept>
#include <utility>

using namespace erlang_aot;
using namespace erlang_aot::codegen;

// Keep lifetime and diagnostic checks active independently of LLVM/build assertions.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Leave the source manager and parser behind to prove the request owns syntax provenance.
CompilationRequest request(const std::string &filename) {
    SourceManager sources;
    PreprocessorSession preprocessor(sources.add(filename, "-define(V,42). f() -> ?V."));
    auto parsed = parse_module(preprocessor);
    require(parsed.succeeded(), "fixture parse failed");
    CompilationRequest batch;
    batch.inputs.emplace_back(filename, std::move(parsed.module));
    batch.project_target = "app";
    batch.target_triple = "aarch64-unknown-linux-gnu";
    batch.optimization = OptimizationLevel::speed;
    batch.disable_type_specialization = true;
    return batch;
}

// Exercise the real LLVM callback with a diagnostic whose text expires on return.
void diagnose(Compilation &compilation, llvm::DiagnosticSeverity severity, const std::string &text) {
    const llvm::Twine borrowed(text);
    const llvm::DiagnosticInfoGeneric diagnostic(borrowed, severity);
    detail::state(compilation).context->diagnose(diagnostic);
}

// Verify input metadata, AST origins and empty IR without advancing to target selection.
void check_input(const Compilation &compilation) {
    const auto &batch = compilation.request();
    require(batch.project_target == "app" && batch.target_triple == "aarch64-unknown-linux-gnu", "options lost");
    require(batch.optimization == OptimizationLevel::speed && batch.disable_type_specialization, "policy lost");
    const auto &syntax = batch.inputs.front().syntax;
    const auto &function = std::get<ast::Function>(syntax.form(syntax.forms().back()).value);
    const auto &expression = syntax.expression(function.clauses.front().body.front());
    require(std::get<ast::IntegerLiteral>(expression.value).value.decimal == "42", "syntax lost");
    require(syntax.anchor(expression.source).location.file == "owned.erl", "source provenance lost");
    const auto &module = *detail::state(compilation).modules.front();
    require(module.getName() == "owned.erl" && module.empty(), "unexpected lowering or module identity");
    require(module.getTargetTriple().str().empty() && module.getDataLayoutStr().empty(), "step 4 ran early");
}

// Moves preserve both AST/IR addresses and the callback destination, including move assignment.
void check_moves() {
    Compilation original(request("owned.erl"));
    const auto *context = detail::state(original).context.get();
    const auto *module = detail::state(original).modules.front().get();
    const auto *syntax = &original.request().inputs.front().syntax;
    Compilation moved(std::move(original));
    original = Compilation(request("reused.erl"));
    require(original.request().inputs.front().source_path == "reused.erl", "moved owner could not be reused");
    check_input(moved);
    Compilation assigned(request("discarded.erl"));
    assigned = std::move(moved);
    require(detail::state(assigned).context.get() == context, "context moved in memory");
    require(detail::state(assigned).modules.front().get() == module, "module moved in memory");
    require(&assigned.request().inputs.front().syntax == syntax, "syntax owner moved in memory");
    diagnose(assigned, llvm::DS_Warning, "warning after move");
    const auto result = std::move(assigned).take_result();
    require(result.diagnostics().front().message == "warning after move", "callback destination was stale");
    require(result.status() == CompilationStatus::incomplete, "ownership operations reported compilation success");
}

// Separate batches may reuse LLVM type names while keeping modules, outputs and errors isolated.
void check_independent_instances() {
    Compilation first(request("first.erl"));
    auto second_request = request("second.erl");
    second_request.inputs.emplace_back("third.erl", ast::Module{});
    Compilation second(std::move(second_request));
    auto &first_state = detail::state(first);
    auto &second_state = detail::state(second);
    const auto *first_type = llvm::StructType::create(*first_state.context, "same_name");
    const auto *second_type = llvm::StructType::create(*second_state.context, "same_name");
    require(first_type != second_type && first_type->getName() == second_type->getName(), "contexts shared types");
    require(second_state.modules.size() == 2, "batch module order lost");
    require(&second_state.modules.back()->getContext() == second_state.context.get(), "module has wrong context");
    require(first.result().add_output({"first", OutputKind::llvm_ir, {std::byte{1}}}), "staging failed");
    diagnose(first, llvm::DS_Error, "owned LLVM error");
    diagnose(second, llvm::DS_Note, "owned LLVM note");
    diagnose(second, llvm::DS_Remark, "owned LLVM remark");
    require(first.result().status() == CompilationStatus::failed, "LLVM error did not propagate");
    require(first.result().outputs().empty() && !first.result().complete(), "LLVM failure retained output");
    require(second.result().status() == CompilationStatus::incomplete, "failure crossed contexts");
    const auto result = std::move(second).take_result();
    require(result.diagnostics().size() == 2, "LLVM notes were lost or duplicated");
    require(result.diagnostics().front().level == DiagnosticLevel::note, "note changed severity");
    require(result.diagnostics().back().message == "owned LLVM remark", "temporary LLVM text escaped");
}

// Inject formatting failure inside our callback, without requiring actual memory exhaustion.
class FailingDiagnostic final : public llvm::DiagnosticInfo {
  public:
    // Use a plugin diagnostic kind so the context routes this test diagnostic to our callback.
    FailingDiagnostic() : DiagnosticInfo(llvm::DK_FirstPluginKind, llvm::DS_Error) {}

    // Simulate failure while copying a diagnostic, before any result message can be retained.
    void print(llvm::DiagnosticPrinter &) const override { throw std::bad_alloc(); }
};

// Reporting failure must remain observable and must never unwind through the exception-free SDK.
void check_callback_failure() {
    Compilation compilation(request("failure.erl"));
    detail::state(compilation).context->diagnose(FailingDiagnostic{});
    const auto result = std::move(compilation).take_result();
    require(result.status() == CompilationStatus::failed && result.diagnostic_capture_failed(),
            "callback exception did not latch failure");
    require(result.diagnostics().empty(), "callback fabricated a diagnostic after allocation failure");
}

// Run independent lifetime scenarios while reporting failures without terminating the test host.
int main() {
    try {
        check_moves();
        check_independent_instances();
        check_callback_failure();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

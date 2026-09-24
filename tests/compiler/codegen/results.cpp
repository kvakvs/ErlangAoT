#include "codegen/compilation.hpp"
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

using namespace erlang_aot::codegen;
static_assert(!std::is_copy_constructible_v<CompilationRequest>);
static_assert(!std::is_copy_constructible_v<CompilationInput>);
static_assert(!std::is_copy_constructible_v<CompilationResult>);
static_assert(!std::is_copy_constructible_v<Compilation>);
static_assert(std::is_nothrow_move_constructible_v<CompilationRequest>);
static_assert(std::is_nothrow_move_assignable_v<Compilation>);
static_assert(std::is_nothrow_move_constructible_v<CompilationResult>);

// Keep assertions active in release builds and provide a useful test failure message.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Use synthetic bytes to test ownership without claiming object emission is implemented.
OutputBuffer output() { return {"answer", OutputKind::object, {std::byte{0x41}, std::byte{0}, std::byte{0xff}}}; }

// Transfer warnings and binary bytes to an independent result after the producer is destroyed.
CompilationResult successful_result() {
    Compilation compilation(CompilationRequest{});
    auto &result = compilation.result();
    require(result.status() == CompilationStatus::incomplete, "new owner reported success");
    result.report({DiagnosticLevel::warning, "owned warning", erlang_aot::LogicalLocation{"a.erl", 2, 3}, "a"});
    require(result.add_output(output()), "incomplete result refused output");
    require(result.complete(), "warning incorrectly failed compilation");
    require(!result.add_output(output()), "completed result accepted more output");
    return std::move(compilation).take_result();
}

// Check data and source coordinates after all producer-side state has been released.
void check_successful_result() {
    auto result = successful_result();
    const auto &diagnostic = result.diagnostics().front();
    require(result.status() == CompilationStatus::succeeded, "success was lost during transfer");
    require(diagnostic.message == "owned warning" && diagnostic.module_name == "a", "diagnostic text lost");
    require(diagnostic.location && diagnostic.location->file == "a.erl" && diagnostic.location->column == 3,
            "location lost");
    require(result.outputs().front().bytes == output().bytes, "binary bytes were truncated or lost");
    CompilationResult destination;
    destination = std::move(result);
    require(destination.outputs().front().module_name == "answer", "result move assignment lost outputs");
}

// An error invalidates all batch outputs and cannot be undone by a completion request.
void check_failed_result() {
    CompilationResult result;
    require(result.add_output(output()), "output staging failed");
    result.report({DiagnosticLevel::error, "rejected module", std::nullopt, "bad"});
    require(result.status() == CompilationStatus::failed && result.outputs().empty(), "error retained output");
    require(!result.complete() && !result.add_output(output()), "failure was not latched");
    require(result.diagnostics().size() == 1, "propagation duplicated an error");
    CompilationResult completed;
    require(completed.complete(), "empty bookkeeping result could not complete");
    completed.report({DiagnosticLevel::error, "late failure", std::nullopt, {}});
    require(completed.status() == CompilationStatus::failed, "late failure disappeared");
}

// Compile this consumer without LLVM include paths, exercising the hidden implementation boundary.
int main() {
    try {
        check_successful_result();
        check_failed_result();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

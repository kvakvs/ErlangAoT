#pragma once
#include "output.hpp"
#include <cstdint>
#include <erlang_aot/compiler/diagnostic.hpp>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace erlang_aot::codegen {
enum class DiagnosticLevel : std::uint8_t { note, warning, error };
enum class CompilationStatus : std::uint8_t { incomplete, succeeded, failed };

struct CompilationDiagnostic {
    // Preserve backend notes separately from warnings and compilation-failing errors.
    DiagnosticLevel level = DiagnosticLevel::error;
    // Copy diagnostic text; LLVM diagnostic objects may borrow temporary strings.
    std::string message;
    // Retain available source coordinates by value, without requiring a live AST.
    std::optional<LogicalLocation> location;
    // Attach semantic module identity when known; context-wide SDK diagnostics leave it empty.
    std::string module_name;
};

// Own a batch outcome independently of its compiler state; inspection spans borrow this owner.
class CompilationResult {
  public:
    // Start incomplete so constructing an owner cannot masquerade as successful compilation.
    CompilationResult() = default;
    // Transfer buffered results without duplicating their ownership.
    CompilationResult(CompilationResult &&) noexcept = default;
    CompilationResult &operator=(CompilationResult &&) noexcept = default;
    CompilationResult(const CompilationResult &) = delete;
    CompilationResult &operator=(const CompilationResult &) = delete;
    ~CompilationResult() = default;

    // Inspect status and owned snapshots without exposing mutable result containers.
    CompilationStatus status() const;
    std::span<const CompilationDiagnostic> diagnostics() const;
    std::span<const OutputBuffer> outputs() const;
    bool diagnostic_capture_failed() const;
    // Retain a diagnostic and latch errors, discarding artifacts from the failed batch.
    void report(CompilationDiagnostic diagnostic);
    // Stage bytes only while the batch is incomplete; never publish files here.
    bool add_output(OutputBuffer output);
    // Record that the caller completed its phases, unless an earlier failure was latched.
    bool complete();
    // Record callback allocation/formatting failure without allocating or unwinding through LLVM.
    void fail_diagnostic_capture() noexcept;

  private:
    // Preserve failure even if later pipeline stages attempt to mark the batch complete.
    CompilationStatus status_ = CompilationStatus::incomplete;
    // Retain diagnostics in reporting order across module and context destruction.
    std::vector<CompilationDiagnostic> diagnostics_;
    // Own staged bytes until release or an error invalidates the entire batch.
    std::vector<OutputBuffer> outputs_;
    // Make lost diagnostic details observable when reporting itself fails.
    bool diagnostic_capture_failed_ = false;
};
} // namespace erlang_aot::codegen

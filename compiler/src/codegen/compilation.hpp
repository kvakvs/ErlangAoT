#pragma once
#include "request.hpp"
#include "result.hpp"
#include <memory>

namespace erlang_aot::codegen {
class Compilation;

namespace detail {
struct CompilationState;
// Give backend implementation files checked access to LLVM state without exposing it to drivers.
CompilationState &state(Compilation &compilation);
const CompilationState &state(const Compilation &compilation);
} // namespace detail

// Move-only batch owner; no LLVM types or behavior leak into frontend/runtime interfaces.
class Compilation {
  public:
    // Own the request and create empty LLVM modules; no lowering or target selection occurs.
    explicit Compilation(CompilationRequest request);
    // Destroy modules before their context and keep diagnostic storage alive through both.
    ~Compilation();
    // Move stable heap state so LLVM's diagnostic callback retains its original destination.
    Compilation(Compilation &&) noexcept;
    Compilation &operator=(Compilation &&) noexcept;
    Compilation(const Compilation &) = delete;
    Compilation &operator=(const Compilation &) = delete;
    // Borrow input syntax/options and the accumulating result while this owner remains live.
    const CompilationRequest &request() const;
    CompilationResult &result();
    const CompilationResult &result() const;
    // Consume the owner and destroy LLVM state, returning independently owned result data.
    CompilationResult take_result() &&;

  private:
    friend detail::CompilationState &detail::state(Compilation &compilation);
    friend const detail::CompilationState &detail::state(const Compilation &compilation);
    // Keep nonmovable LLVM context/module state at a stable address across owner moves.
    std::unique_ptr<detail::CompilationState> state_;
};
} // namespace erlang_aot::codegen

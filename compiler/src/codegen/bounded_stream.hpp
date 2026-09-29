#pragma once
#include <cstddef>
#include <llvm/Support/raw_ostream.h>
#include <vector>

namespace erlang_aot::codegen {
// Let LLVM serialize normally while bounding retained bytes before each allocation.
class BoundedStream final : public llvm::raw_pwrite_stream {
  public:
    // Disable buffering so every write observes the same checked byte ceiling.
    explicit BoundedStream(std::size_t limit);
    // Transfer complete bytes only after the writer/passes have finished successfully.
    std::vector<std::byte> take_bytes();

  private:
    // Own serialized bytes independently of LLVM module/context lifetimes.
    std::vector<std::byte> bytes_;
    // Reject growth beyond the smaller module or remaining batch allowance.
    std::size_t limit_;
    // Preserve the writer's logical cursor after overflow while discarding further bytes.
    std::uint64_t position_ = 0;
    // Latch errors without unwinding through an LLVM SDK potentially built without exceptions.
    enum class Failure : std::uint8_t { none, limit, patch, allocation };
    Failure failure_ = Failure::none;
    // Append or patch checked ranges without allowing overflow or unchecked resize.
    void write_impl(const char *data, std::size_t size) override;
    void pwrite_impl(const char *data, std::size_t size, std::uint64_t offset) override;
    // Report the logical unbuffered write cursor expected by LLVM object writers.
    std::uint64_t current_pos() const override;
};
} // namespace erlang_aot::codegen

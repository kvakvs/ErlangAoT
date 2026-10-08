#include "spawn.hpp"

// The driver of spawned programs (docs/ports.md#drivers): the I/O thread reads the program's stdout, writes queued
// output to its stdin, closes both pipes when done and reports the program's exit status.
namespace clause::runtime::detail {
namespace {
class SpawnDriver final : public PortDriver {
  public:
    explicit SpawnDriver(const Spawned &spawned) noexcept : spawned_(spawned) {}

    // Output always goes through the I/O thread's queue (queued_output), never written here.
    std::expected<void, DriverError> write(std::span<const std::byte>) override {
        return std::unexpected(DriverError{"einval"});
    }

    std::optional<Descriptor> input() const noexcept override { return pipe(spawned_.input); }

    std::optional<Descriptor> queued_output() const noexcept override { return pipe(spawned_.output); }

    std::optional<std::int64_t> child() const noexcept override { return spawned_.child; }

    std::optional<std::int64_t> os_pid() const noexcept override { return spawned_.os_pid; }

  private:
    // A pipe end the I/O thread owns, or none for a direction the port does not use.
    static std::optional<Descriptor> pipe(NativeHandle handle) noexcept {
        if (handle == -1) {
            return std::nullopt;
        }
        return Descriptor{.handle = handle, .owned = true, .overlapped = true};
    }

    // The started program and its pipes.
    Spawned spawned_;
};
} // namespace

std::expected<std::unique_ptr<PortDriver>, DriverError> spawn_driver(const SpawnRequest &request,
                                                                     const PortOptions &options) {
    const auto spawned = spawn_program(request, options);
    if (!spawned) {
        return std::unexpected(spawned.error());
    }
    return std::make_unique<SpawnDriver>(*spawned);
}
} // namespace clause::runtime::detail

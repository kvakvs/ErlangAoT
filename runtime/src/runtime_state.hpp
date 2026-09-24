#pragma once
#include <erlang_aot/runtime/runtime.hpp>
#include <vector>

namespace erlang_aot::runtime {
// Runtime-wide services are reserved, not instantiated; future module/atom work supplies their definitions.
class Runtime::Impl final {
  public:
    // Preserve validated limits and the unique runtime identity before creating any contexts.
    Impl(RuntimeOptions options, std::uint64_t identity);
    // These empty ownership slots outlive all contexts; accessors remain undefined until services exist.
    std::shared_ptr<AtomStorage> atom_storage;
    // Destroy future code registrations/atom roots before the runtime-wide atom table.
    std::shared_ptr<CodeServer> code_server;
    // Retain admission policy and the runtime portion of each immutable process identity.
    RuntimeOptions options;
    std::uint64_t identity;
    // Advance only after successful context publication; never reuse an issued serial.
    std::uint64_t next_context = 1;
    // Destroy process owners before the above service bindings; no signals or workers exist yet.
    std::vector<std::unique_ptr<ProcessContext>> contexts;
};
} // namespace erlang_aot::runtime

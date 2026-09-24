#pragma once
#include <erlang_aot/runtime/code_server.hpp>
#include <erlang_aot/runtime/runtime.hpp>
#include <erlang_aot/runtime/scheduler.hpp>
#include <vector>

namespace erlang_aot::runtime {
// Contexts die before code registrations; atom services remain reserved.
class Runtime::Impl final {
  public:
    // Preserve validated limits and the unique runtime identity before creating any contexts.
    Impl(RuntimeOptions options, std::uint64_t identity);
    // Retire scheduling records before contexts, then release code while the stopped service still exists.
    ~Impl();
    // Atom storage remains reserved until runtime atom initialization is implemented.
    std::shared_ptr<AtomStorage> atom_storage;
    // Own one host-serialized lifecycle service; no worker pool or process continuations exist yet.
    SchedulerService scheduler;
    // Own one server and destroy registrations before the future atom table.
    CodeServer code_server;
    // Retain admission policy and the runtime portion of each immutable process identity.
    RuntimeOptions options;
    std::uint64_t identity;
    // Advance only after successful context publication; never reuse an issued serial.
    std::uint64_t next_context = 1;
    // Destroy process owners before the above service bindings; no signals or workers exist yet.
    std::vector<std::unique_ptr<ProcessContext>> contexts;
};
} // namespace erlang_aot::runtime

# Generated module registration

ABI revision 4 `modules.hpp` describes immutable module/export metadata using fixed version
and word-width fields, target-sized counts, borrowed UTF-8 bytes and generic entry
pointers. LLVM constructs layouts from the target word type; native compile-time
layout assertions and a separately Clang-linked consumer check agreement.

Each module emits `<encoded module/empty function/0>.descriptor` and `.register`.
The registration entry accepts an active `Runtime*` as an opaque pointer and returns
a `Status` byte. Call it explicitly before resolving/invoking exports. The runtime
service copies metadata, validates version/width, checks all exports, builds one
unique all-Term registry and freezes it before publication. Duplicate modules never
replace existing code; malformed or duplicate exports discard the entire draft.
Descriptor spelling bytes are copied during registration. The immutable descriptor
address remains the atom-binding key and its image must stay pinned for the loaded
module lifetime; native code must remain executable for that lifetime.

Generated entries call the native C++ function `erlang_aot_register_module_v4`.
The backend emits its Itanium or Microsoft C++ linker spelling for the target;
this is a project ABI contract, not a C wrapper. LLVM `llvm.used` retains startup
and descriptor symbols, whose references retain the service dependency. A linked
consumer must use `ErlangAoT::generated_program`; the negative link test contains
only generated objects and an empty main and verifies the missing service symbol.
Static archive users must explicitly reference module registration entries to
extract their object files. There are no implicit global constructors.

`runtime::register_module` additionally accepts a retained `CodeImage` for future
loader ownership. Generated startup uses a linked-program image. `ResolvedFunction`
pins the image and frozen registry, including after runtime teardown. Host calls
marshal checked immediate and owned atom words, pass the actual runtime-owned context, and validate results
through the existing checked invocation boundary. Native direct entries require
the caller to obey the live-context/valid-term ABI and establish a checked invocation
scope. See [generated-call failures](generated-call-failures.md) for the mandatory
channel checks, structured errors, cleanup and revision-1/2 descriptor rejection.

[Atom bindings](runtime-atoms.md) are initialized transactionally with the registry.
The descriptor contains literal spellings/slots; module/export names share the same
bounded runtime table. Failed registration retains validated interned spellings but
publishes neither a module nor bindings. Host handles pin immutable atom spellings.
Dynamic loaders, concurrent publication and production startup remain deferred.

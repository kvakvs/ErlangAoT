# Generated module registration

ABI v1 `modules.hpp` describes immutable module/export metadata using fixed version
and word-width fields, target-sized counts, borrowed UTF-8 bytes and generic entry
pointers. LLVM constructs layouts from the target word type; native compile-time
layout assertions and a separately Clang-linked consumer check agreement.

Each module emits `<encoded module/empty function/0>.descriptor` and `.register`.
The registration entry accepts an active `Runtime*` as an opaque pointer and returns
a `Status` byte. Call it explicitly before resolving/invoking exports. The runtime
service copies metadata, validates version/width, checks all exports, builds one
unique all-Term registry and freezes it before publication. Duplicate modules never
replace existing code; malformed or duplicate exports discard the entire draft.
Borrowed descriptor storage must remain valid during registration; native code
must remain executable for the loaded module's lifetime.

Generated entries call the native C++ function `erlang_aot_register_module_v1`.
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
marshal immediate words, pass the actual runtime-owned context, and validate results
through the existing checked invocation boundary. Native direct entries require
the caller to obey the live-context/valid-term ABI.

AtomStorage initialization and module atom roots remain reserved. Metadata names
are owned strings, never compiler-assigned atom IDs; this support does not admit
atom expressions. Dynamic loaders, concurrent publication, production startup and
CLI artifact integration remain outside this step.

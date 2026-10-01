#pragma once
#include <erlang_aot/runtime/modules.hpp>

namespace erlang_aot::runtime {
// Validate every spelling before interning, then stage immutable bindings for atomic module publication.
CodeResult<std::shared_ptr<const ModuleAtoms>> bind_atoms(AtomStorage &storage,
                                                          const abi::v1::ModuleDescriptor &descriptor);
} // namespace erlang_aot::runtime

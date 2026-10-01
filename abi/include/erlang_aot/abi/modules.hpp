#pragma once
#include "status.hpp"
#include "v1.hpp"
#include <cstddef>
#include <type_traits>

namespace erlang_aot::abi::v1 {
struct ExportDescriptor {
    // Borrow exact UTF-8 bytes; registration copies metadata before returning.
    const char *name;
    std::size_t name_size;
    // Resolve the generic entry's fixed Erlang arity independently of its machine signature.
    std::size_t arity;
    GeneratedFunction *entry;
};

struct ModuleDescriptor {
    // Reject incompatible layouts before reading any borrowed metadata.
    std::uint32_t abi_version;
    std::uint32_t term_bits;
    // Borrow exact module spelling and a contiguous, immutable export table.
    const char *name;
    std::size_t name_size;
    const ExportDescriptor *exports;
    std::size_t export_count;
};

// Explicit startup entry; the pointer must designate an active runtime owner.
using GeneratedRegistration = std::uint8_t(void *);
static_assert(std::is_standard_layout_v<ExportDescriptor>);
static_assert(std::is_standard_layout_v<ModuleDescriptor>);
static_assert(sizeof(ExportDescriptor) == 4 * sizeof(TermWord));
static_assert(offsetof(ExportDescriptor, entry) == 3 * sizeof(TermWord));
static_assert(sizeof(ModuleDescriptor) == 8 + 4 * sizeof(TermWord));
static_assert(offsetof(ModuleDescriptor, name) == 8);
static_assert(offsetof(ModuleDescriptor, exports) == 8 + 2 * sizeof(TermWord));
} // namespace erlang_aot::abi::v1

// Generated service boundary uses native C++ linkage with a fixed, documented linker spelling.
// Borrow a Runtime and ModuleDescriptor; contain all exceptions and return a v1::Status byte.
std::uint8_t erlang_aot_register_module_v2(void *runtime, const void *descriptor) noexcept;

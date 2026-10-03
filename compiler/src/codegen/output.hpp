#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace erlang_aot::codegen {
enum class OutputKind : std::uint8_t { object, llvm_ir, llvm_bitcode };

struct OutputBuffer {
    // Identify the semantic module for later artifact naming, independently of its source path.
    std::string module_name;
    // Describe the owned bytes without relying on a filename extension or the host platform.
    OutputKind kind = OutputKind::object;
    // Retain serialized bytes after LLVM modules and their contexts have been destroyed.
    std::vector<std::byte> bytes;
    // Mark the batch's startup object, published as `eav1_start` instead of a module name.
    bool startup = false;
};
} // namespace erlang_aot::codegen

#pragma once
#include "status.hpp"
#include "v1.hpp"
#include <cstddef>

namespace erlang_aot::abi::v1 {
// Dispatch synchronously through a live context; names/words borrow valid arrays (null args only at arity zero).
// Return explicit status and write result only on OK; errors never become words or escape as C++ exceptions.
Status dispatch_builtin(Context *context, const char *module, std::size_t module_size, const char *function,
                        std::size_t function_size, const TermWord *arguments, std::size_t arity,
                        TermWord *result) noexcept;
} // namespace erlang_aot::abi::v1

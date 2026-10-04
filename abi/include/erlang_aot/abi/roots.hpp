#pragma once
#include "v1.hpp"
#include <cstddef>

// Push a zero-initialized root frame on the process stack for one generated call; null preserves a checked failure.
erlang_aot::abi::v1::TermWord *erlang_aot_roots_enter_v4(void *context, std::size_t count) noexcept;
// Transfer an admitted result to the parent/host root before releasing this frame; null frames need no cleanup.
std::uint8_t erlang_aot_roots_leave_v4(void *context, erlang_aot::abi::v1::TermWord *frame,
                                       erlang_aot::abi::v1::TermWord result) noexcept;

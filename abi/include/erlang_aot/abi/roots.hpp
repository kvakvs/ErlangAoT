#pragma once
#include "v1.hpp"
#include <cstddef>

// Push a zero-initialized root frame on the process stack for one generated call; null preserves a checked failure.
// `frame` (an abi::v1::FrameDescriptor) names the function in stack traces; null frames are left out of them.
erlang_aot::abi::v1::TermWord *erlang_aot_roots_enter_v5(void *context, std::size_t count, const void *frame) noexcept;
// Transfer an admitted result to the parent/host root before releasing this frame; null frames need no cleanup.
std::uint8_t erlang_aot_roots_leave_v4(void *context, erlang_aot::abi::v1::TermWord *frame,
                                       erlang_aot::abi::v1::TermWord result) noexcept;

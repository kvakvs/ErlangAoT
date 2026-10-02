#pragma once
#include "immediate_services.hpp"

// Decode eight network-order IEEE binary64 bytes without native floating calling-convention assumptions.
std::uint8_t erlang_aot_float_v1(void *context, const char *bytes, std::size_t size,
                                 erlang_aot::abi::v1::TermWord *output) noexcept;

#pragma once
#include "immediate_services.hpp"

// Print one admitted term as an erlang:display/1 line on standard output; success yields the atom true.
// Rendering or write failures are infrastructure statuses in the checked channel, never Erlang exceptions.
std::uint8_t erlang_aot_display_v1(void *context, erlang_aot::abi::v1::TermWord term,
                                   erlang_aot::abi::v1::TermWord *output) noexcept;

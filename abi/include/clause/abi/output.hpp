#pragma once
#include "immediate_services.hpp"

// Print one admitted term as an erlang:display/1 line on standard output; success yields the atom true.
// Rendering or write failures are infrastructure statuses in the checked channel, never Erlang exceptions.
std::uint8_t CLAUSE_display_v1(void *context, clause::abi::v1::TermWord term,
                               clause::abi::v1::TermWord *output) noexcept;

#pragma once
#include "immediate_services.hpp"

// Materialize bounded canonical decimal text in the receiving context, publishing only a rooted success word.
std::uint8_t CLAUSE_integer_v1(void *context, const char *digits, std::size_t size,
                               clause::abi::v1::TermWord *output) noexcept;

#pragma once
#include "immediate_services.hpp"
#include <cstddef>

namespace erlang_aot::abi::v1 {
// Lists carry their final tail as the last argument, so proper and improper spines share one constructor.
enum class ContainerConstruction : std::uint8_t { tuple, list };
// Shape mismatch is a semantic outcome; ownership and infrastructure failures use the checked channel.
enum class ContainerInspection : std::uint8_t { tuple_shape, tuple_element, cons_shape, cons_head, cons_tail };
} // namespace erlang_aot::abi::v1

// Borrow rooted input words and publish a fully initialized result only on success.
std::uint8_t erlang_aot_construct_v1(void *context, std::uint8_t operation, const erlang_aot::abi::v1::TermWord *values,
                                     std::size_t count, erlang_aot::abi::v1::TermWord *output) noexcept;
// Prove ownership and shape before checking an arity/index or extracting an owned child.
std::uint8_t erlang_aot_inspect_v1(void *context, std::uint8_t operation, erlang_aot::abi::v1::TermWord value,
                                   std::size_t index, erlang_aot::abi::v1::TermWord *output) noexcept;

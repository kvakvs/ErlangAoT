#pragma once

// Optional spelling for a typed std::function registration.
#include "callable.hpp"

namespace erlang_aot::runtime {
// Pass exact value arguments and return CallResult<Term>; callers wrap other result types explicitly.
template <CallableArgument... Arguments> using NativeCallable = TypedCallable<Arguments...>;
} // namespace erlang_aot::runtime

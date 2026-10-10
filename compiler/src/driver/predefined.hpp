#pragma once
#include <clause/compiler/parser.hpp>

namespace clause::cli {
// Append OTP's predefined behaviour_info/1 to a module that declares -callback; returns whether it was added.
bool add_behaviour_info(const ParserSession &parser, const FeatureSnapshot &features);
} // namespace clause::cli

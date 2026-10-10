#pragma once
#include <clause/compiler/parser.hpp>
#include <filesystem>

namespace clause::cli {
// Append OTP's predefined functions: behaviour_info/1 for a module declaring -callback, then module_info/0,1.
// Returns the number of forms appended.
std::size_t add_predefined(const ParserSession &parser, const FeatureSnapshot &features,
                           const std::filesystem::path &path, bool escript);
} // namespace clause::cli

#pragma once
// Added for parse transforms: External Term Format, the loader's request and reply encoding.
#include "terms.hpp"
#include <string>
#include <string_view>

namespace clause::transforms {
// Encode one term as term_to_binary/1 does (version 131, uncompressed), choosing the same tags.
std::string encode_external(const Terms &terms, TermId id);
// Decode one complete binary_to_term/1 encoding into the arena; malformed or unsupported data throws TermError.
TermId decode_external(std::string_view bytes, Terms &terms);
} // namespace clause::transforms

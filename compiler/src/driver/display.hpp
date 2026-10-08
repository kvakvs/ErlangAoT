#pragma once
#include <string>
#include <string_view>

namespace clause::cli {
// Quote control bytes and delimiters so user-controlled names cannot inject inspection or trace lines.
std::string quote_text(std::string_view text);
} // namespace clause::cli

#pragma once
#include "model.hpp"
#include <stdexcept>

namespace erlang_aot::project {
// Format project errors without writing to a global output stream.
std::string render(const Error &error);

class Failure : public std::runtime_error {
  public:
    // Retain structured failure data alongside the human-readable message.
    explicit Failure(Error error);
    // Retain machine-readable context for command exit handling.
    Error detail;
};

// Stop the current project operation with owned diagnostic context.
[[noreturn]] void fail(const Site &site, std::string message, int exit_code = 1);
} // namespace erlang_aot::project

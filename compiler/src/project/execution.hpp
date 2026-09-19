#pragma once
#include "plan.hpp"
#include <functional>

namespace erlang_aot::project {
// Consume diagnostic text synchronously; callers may copy it for later inspection.
using MessageSink = std::function<void(std::string_view)>;
// Supply the shared per-file frontend without depending on concrete driver implementation.
using FileExecutor =
    std::function<bool(const std::filesystem::path &, const PreprocessorOptions &, const MessageSink &)>;
// Execute all planned files in target order and latch failures without writing executables.
int execute(const Invocation &invocation, const FileExecutor &executor, const MessageSink &diagnostics);
} // namespace erlang_aot::project

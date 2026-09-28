#pragma once
#include "plan.hpp"
#include <functional>
#include <span>

namespace erlang_aot::project {
// Consume diagnostic text synchronously; callers may copy it for later inspection.
using MessageSink = std::function<void(std::string_view)>;
// Supply the shared batch frontend without depending on concrete driver implementation.
using TargetExecutor =
    std::function<bool(std::span<const std::filesystem::path>, const PreprocessorOptions &, const MessageSink &)>;
// Execute all planned files in target order and latch failures without writing executables.
int execute(const Invocation &invocation, const TargetExecutor &executor, const MessageSink &diagnostics);
} // namespace erlang_aot::project

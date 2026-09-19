#pragma once
#include "cli.hpp"
#include "execution.hpp"
#include <ostream>

namespace erlang_aot::project {
// Load, prepare, and execute a project through the caller's existing frontend boundary.
int run(const Request &request, const PlanOptions &options, const FileExecutor &executor, std::ostream &diagnostics);
} // namespace erlang_aot::project

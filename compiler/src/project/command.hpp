#pragma once
#include "cli.hpp"
#include "execution.hpp"
#include <ostream>

namespace clause::project {
// Load, prepare, and execute a project through the caller's existing frontend boundary.
int run(const Request &request, const PlanOptions &options, const TargetExecutor &executor, std::ostream &output,
        std::ostream &diagnostics);
} // namespace clause::project

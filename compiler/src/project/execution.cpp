#include "execution.hpp"
#include "diagnostics.hpp"
#include <utility>

namespace clause::project {
namespace {
// Name the project and target once, on a line of their own before the target's first diagnostic; the shared
// frontend retains each diagnostic's source.
bool process(const Invocation &invocation, const PlannedTarget &target, const TargetExecutor &executor,
             const MessageSink &diagnostics) {
    bool named = false;
    const MessageSink report = [&](const std::string_view message) {
        if (!std::exchange(named, true)) {
            diagnostics(where({invocation.file, "", target.name, 0, 0}) + ':');
        }
        diagnostics(message);
    };
    try {
        return executor(invocation, target, report);
    } catch (const std::exception &error) {
        report("error: " + std::string(error.what()));
    }
    return true;
}
} // namespace

int execute(const Invocation &invocation, const TargetExecutor &executor, const MessageSink &diagnostics) {
    bool failed = false;
    for (const auto &target : invocation.targets) {
        failed = process(invocation, target, executor, diagnostics) || failed;
    }
    return failed ? 1 : 0;
}
} // namespace clause::project

#include "execution.hpp"
#include "diagnostics.hpp"

namespace erlang_aot::project {
namespace {
// Attach project/target context; the shared frontend retains each diagnostic's source.
bool process(const Invocation &invocation, const PlannedTarget &target, const TargetExecutor &executor,
             const MessageSink &diagnostics) {
    const MessageSink report = [&](std::string_view message) {
        diagnostics(render({{invocation.file, "", target.name, 0, 0}, std::string(message), 1}));
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
} // namespace erlang_aot::project

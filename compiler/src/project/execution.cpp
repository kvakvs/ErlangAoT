#include "execution.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"

namespace erlang_aot::project {
namespace {
// Attach owning project, target, and physical source context to every frontend diagnostic.
bool process(const Invocation &invocation, const PlannedTarget &target, const std::filesystem::path &path,
             const FileExecutor &executor, const MessageSink &diagnostics) {
    const MessageSink report = [&](std::string_view message) {
        diagnostics(render({{invocation.file, path_text(path), target.name, 0, 0}, std::string(message), 1}));
    };
    try {
        return executor(path, target.preprocessing, report);
    } catch (const std::exception &error) {
        report("error: " + std::string(error.what()));
    }
    return true;
}
} // namespace

int execute(const Invocation &invocation, const FileExecutor &executor, const MessageSink &diagnostics) {
    if (!invocation.frontend) {
        diagnostics("error: compilation is not implemented yet; no output was written.");
        return 1;
    }
    bool failed = false;
    for (const auto &target : invocation.targets) {
        for (const auto &path : target.sources) {
            failed = process(invocation, target, path, executor, diagnostics) || failed;
        }
    }
    return failed ? 1 : 0;
}
} // namespace erlang_aot::project

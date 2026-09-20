#include "project/execution.hpp"
#include "support.hpp"
using namespace erlang_aot;
using namespace erlang_aot::project;

// Verify context, target/file ordering, and failure latching through an injected frontend.
int main() {
    Invocation invocation;
    invocation.file = "project.toml";
    invocation.frontend = true;
    PlannedTarget first;
    first.name = "app";
    first.sources = {"shared.erl", "later.erl"};
    first.preprocessing.definitions = {"VALUE=1"};
    auto second = first;
    second.name = "tests";
    second.sources = {"shared.erl"};
    second.preprocessing.definitions = {"VALUE=2"};
    invocation.targets = {first, second};
    std::vector<std::string> calls;
    std::vector<std::string> messages;
    const MessageSink sink = [&](std::string_view text) { messages.emplace_back(text); };
    const FileExecutor executor = [&](const auto &path, const auto &settings, const auto &report) {
        calls.push_back(path.string() + settings.definitions.front());
        report("error: injected");
        return path == "shared.erl";
    };
    require(execute(invocation, executor, sink) == 1);
    require(calls == std::vector<std::string>{"shared.erlVALUE=1", "later.erlVALUE=1", "shared.erlVALUE=2"});
    require(messages.size() == 3);
    require(messages.front().find("project.toml [target app] (shared.erl)") != std::string::npos);
    require(messages.back().find("target tests") != std::string::npos);
    calls.clear();
    messages.clear();
    invocation.frontend = false;
    require(execute(invocation, executor, sink) == 1);
    require(calls == std::vector<std::string>{"shared.erlVALUE=1", "later.erlVALUE=1", "shared.erlVALUE=2"});
    require(messages.size() == 3);
    require(execute(invocation, [](const auto &, const auto &, const auto &) { return false; }, sink) == 0);
    invocation.frontend = true;
    require(execute(invocation, [](const auto &, const auto &, const auto &) { return false; }, sink) == 0);
}

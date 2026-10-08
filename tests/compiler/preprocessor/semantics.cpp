#include <clause/compiler/preprocessor.hpp>
#include <iostream>
#include <stdexcept>

using namespace clause;

struct Result {
    // Capture forms and errors separately while checking the session's final status.
    std::vector<Token> tokens;
    std::vector<Diagnostic> diagnostics;
    bool failed;
};

void require(bool value, const char *message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

Result run(std::string text, PreprocessorOptions options = {}) {
    SourceManager sources;
    PreprocessorSession session(sources.add("module.erl", std::move(text)), std::move(options));
    Result result{{}, {}, false};
    while (auto event = session.next()) {
        if (const auto *form = std::get_if<OrdinaryForm>(&*event)) {
            if (form->tokens.size() > 1 && form->tokens[1].text() == U"file") {
                continue;
            }
            result.tokens.insert(result.tokens.end(), form->tokens.begin(), form->tokens.end());
        }
        if (auto *error = std::get_if<Diagnostic>(&*event)) {
            result.diagnostics.push_back(*error);
        }
    }
    result.failed = session.failed();
    return result;
}

// Tiny budgets and throwing readers are unavailable through the production CLI.
void limits() {
    std::string input = "-define(M0, 0).\n";
    for (unsigned index = 1; index < 80; ++index) {
        input += "-define(M" + std::to_string(index) + ",?M" + std::to_string(index - 1) + ").\n";
    }
    PreprocessorOptions options;
    options.limits.expansion_depth = 20;
    require(run(input + "?M79.", options).diagnostics.front().code == DiagnosticCode::resource_limit,
            "dependency depth budget");
    options.limits.tokens = 30;
    require(run("-define(D(X), {X,X}). ?D(?D(?D(?D(1)))).", options).failed, "produced token budget");
    options.limits.expression_depth = 10;
    require(run("-if((((((((((((true)))))))))))). yes. -endif.", options).failed, "expression depth budget");
    options = {};
    options.limits.include_depth = 1;
    options.read_file = [](const auto &) { return std::optional<std::string>{"-include(\"again.hrl\")."}; };
    require(run("-include(\"again.hrl\").", options).diagnostics.front().code == DiagnosticCode::resource_limit,
            "include depth budget");
}

// An inactive branch must not invoke an embedding application's reader callback at all.
void inactive_read() {
    PreprocessorOptions options;
    options.read_file = [](const auto &) -> std::optional<std::string> {
        throw std::runtime_error("inactive include accessed filesystem");
    };
    require(!run("-if(false). -include(\"missing\"). -endif.", options).failed, "inactive reader called");
}

// Source-level behavior now runs through frontend_cli and preprocessor_workflow.
int main() {
    try {
        limits();
        inactive_read();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

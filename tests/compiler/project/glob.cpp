#include "project/glob.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
using namespace erlang_aot::project;

// Match portable patterns without consulting host path semantics.
bool match(std::string_view pattern, std::string_view path) {
    return matches(parse_glob({std::string(pattern), {}}), path);
}

// Require deterministic errors for malformed patterns and work exhaustion.
template <typename Action> void rejects(Action action) {
    try {
        action();
    } catch (const Failure &) {
        return;
    }
    require(false);
}

// Cover wildcard boundaries, Unicode scalars, hidden files, and bounded adversarial inputs.
int main() {
    require(match("**/*.erl", "a.erl"));
    require(match("**/*.erl", "a/b/c.erl"));
    require(match("**/**/a.erl", "x/a.erl"));
    require(!match("*.erl", "x/a.erl"));
    require(match("?.erl", "λ.erl"));
    require(!match("?.erl", "ab.erl"));
    require(match("*.erl", ".hidden.erl"));
    require(!match("A.erl", "a.erl"));
    require(match("a*?z", "abcz"));
    for (const auto *pattern : {"[a].erl", "{a,b}.erl", "!a.erl", "a**.erl"}) {
        rejects([&] { (void)parse_glob({pattern, {}}); });
    }
    rejects([] { (void)match("*", std::string(1, static_cast<char>(0xff))); });
    rejects([] { (void)matches(parse_glob({"**/*.erl", {}}), "long/path/file.erl", {1}); });
    std::string adversarial;
    for (int i = 0; i < 1000; ++i) {
        adversarial += "*a";
    }
    rejects([&] { (void)matches(parse_glob({adversarial, {}}), std::string(1000, 'a'), {100}); });
}

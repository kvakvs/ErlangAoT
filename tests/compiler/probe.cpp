#include <erlang_aot/compiler/probe.hpp>
#include <stdexcept>

// Keep assertions active in release builds.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("parser integration probe failed");
    }
}

// Cover recursive rules, full consumption, locations, and rollback after
// failure.
int main() {
    const auto parsed = erlang_aot::probe_directive("  -define(X, (a,(b))).");
    require(parsed.has_value());
    require(parsed->name == "define" && parsed->begin == 3 && parsed->end == 9);
    require(!erlang_aot::probe_directive("-define(X,(a)."));
    require(!erlang_aot::probe_directive("-9bad(X)."));
    require(!erlang_aot::probe_directive("-define(X). trailing"));
    require(erlang_aot::probe_directive("-define(X).").has_value());
}

#include <erlang_aot/abi/feature_diagnostic.hpp>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot::abi::v1;

// Check the catalog as a compatibility snapshot, not only against its own lookup implementation.
constexpr std::array<std::string_view, 23> names{"pattern matching",
                                                 "guards",
                                                 "multiple clauses",
                                                 "arithmetic",
                                                 "bignum expressions",
                                                 "atom expressions",
                                                 "heap expressions",
                                                 "dynamic calls",
                                                 "recursive calls",
                                                 "closures",
                                                 "exceptions",
                                                 "receive",
                                                 "behavior-changing attributes",
                                                 "term services",
                                                 "builtins",
                                                 "process execution",
                                                 "message passing",
                                                 "scheduling",
                                                 "allocation",
                                                 "garbage collection",
                                                 "atom collection",
                                                 "dynamic modules",
                                                 "executable linking"};

// Keep assertions active under NDEBUG.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Every stable ID must retain its spelling, ownership metadata and focused reporter coverage.
void check_catalog() {
    require(feature_catalog.size() == names.size(), "update the catalog compatibility snapshot");
    for (std::size_t index = 0; index < names.size(); ++index) {
        const auto *feature = find_feature(static_cast<FeatureId>(index + 1));
        require(feature != nullptr, "stable feature ID missing");
        require(feature->name == names[index], "stable feature spelling changed");
        require(feature->status == FeatureStatus::deferred, "unexpected feature status");
        require(!feature->boundary.empty() && feature->plan_step > 0 && feature->plan_step <= 46,
                "missing owning boundary/plan step");
        const auto test = feature->owner == FeatureOwner::runtime ? "runtime_features" : "codegen_features";
        require(feature->failure_test == test, "missing focused failure test");
    }
    require(find_feature(FeatureId::invalid) == nullptr, "zero became a valid feature");
}

// Partial context omits unknown fields; full context preserves each available dimension.
void check_context() {
    require(format_feature_failure(FeatureId::pattern_matching) == "[pattern matching] notimpl", "base format changed");
    const FeatureContext context{"src/example.erl", 12, 5, "example", "aarch64-unknown-linux-gnu", "match argument"};
    require(format_feature_failure(FeatureId::pattern_matching, context) ==
                "[pattern matching] notimpl: src/example.erl:12:5 [module=\"example\"] "
                "[target=\"aarch64-unknown-linux-gnu\"] [operation=\"match argument\"]",
            "context format changed");
    require(format_feature_failure(FeatureId::guards, {"test.erl"}) == "[guards] notimpl: test.erl",
            "unknown coordinates printed");
    require(format_feature_failure(FeatureId::invalid) == "invalid deferred feature ID 0",
            "unknown ID mislabeled notimpl");
}

// Embedded control bytes, quotes and Windows separators must stay within one rendered line.
void check_escaping() {
    const FeatureContext context{"C:\\source\nfile.erl", 0, 99, "m\"\\\t", {}, std::string_view("x\0y", 3)};
    require(format_feature_failure(FeatureId::guards, context) ==
                "[guards] notimpl: C:\\\\source\\x0afile.erl [module=\"m\\\"\\\\\\x09\"] [operation=\"x\\x00y\"]",
            "context can inject a diagnostic line or lose bytes");
}

// This ABI-only executable proves catalog/formatting need neither compiler nor runtime libraries.
int main() {
    try {
        check_catalog();
        check_context();
        check_escaping();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

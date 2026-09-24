#include <erlang_aot/abi/v1.hpp>
#include <erlang_aot/runtime/features.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace erlang_aot::runtime;
using namespace erlang_aot::abi::v1;

// Preserve assertions in release builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Retain delivered messages independently of the reporter's temporary formatting storage.
bool collect(void *context, std::string_view message) {
    static_cast<std::vector<std::string> *>(context)->emplace_back(message);
    return true;
}

// Simulate an unavailable output destination without throwing through the service boundary.
bool refuse(void *, std::string_view) { return false; }

// Simulate a host callback that violates its normal-delivery contract.
bool throw_sink(void *, std::string_view) { throw std::runtime_error("sink failure"); }

// Every runtime feature reports once; propagation/retries preserve the first explicit failure status.
void check_catalog() {
    for (const auto &feature : feature_catalog) {
        if (feature.owner != FeatureOwner::runtime) {
            continue;
        }
        std::vector<std::string> messages;
        FeatureFailure failure({&messages, collect});
        require(failure.status() == Status::ok && messages.empty(), "unused reporter was noisy");
        require(failure.report(feature.id) == Status::not_implemented, "placeholder returned success");
        require(failure.report(feature.id) == Status::not_implemented, "failure status changed");
        require(messages == std::vector<std::string>{'[' + std::string(feature.name) + "] notimpl"},
                "duplicate/wrong owner report");
        FeatureFailure independent({&messages, collect});
        independent.report(feature.id);
        require(messages.size() == 2, "independent failure was globally suppressed");
    }
}

// Runtime reports include available source/module/target/operation data without depending on LLVM.
void check_context() {
    std::vector<std::string> messages;
    FeatureFailure failure({&messages, collect});
    failure.report(FeatureId::garbage_collection, {"src/example.erl", 12, 5, "example", "native", "collect"});
    require(messages.front() == "[garbage collection] notimpl: src/example.erl:12:5 [module=\"example\"] "
                                "[target=\"native\"] [operation=\"collect\"]",
            "runtime context changed");
    failure.report(FeatureId::allocation);
    require(messages.size() == 1, "propagation replaced the original failure");
}

// I/O/callback errors and unknown IDs remain explicit failures with no fake terms or leaked exceptions.
void check_errors() {
    FeatureFailure rejected({nullptr, refuse});
    require(rejected.report(FeatureId::allocation) == Status::diagnostic_failure, "sink refusal swallowed");
    FeatureFailure throwing({nullptr, throw_sink});
    require(throwing.report(FeatureId::allocation) == Status::diagnostic_failure, "sink exception swallowed");
    require(throwing.report(FeatureId::allocation) == Status::diagnostic_failure, "failed sink retried");
    std::vector<std::string> messages;
    FeatureFailure invalid({&messages, collect});
    require(invalid.report(FeatureId::invalid) == Status::invalid_argument, "invalid ID accepted");
    require(messages == std::vector<std::string>{"invalid deferred feature ID 0"}, "invalid ID mislabeled notimpl");
}

// A generated-service-shaped test boundary returns status only; no C++ exception may cross it.
Status unavailable_service() noexcept {
    FeatureFailure failure;
    failure.report(FeatureId::garbage_collection, {"src/example.erl", 12, 5});
    return failure.report(FeatureId::garbage_collection);
}

// Subprocess modes expose only stderr on failure and remain silent without a reached placeholder.
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "stderr") {
            return unavailable_service() == Status::not_implemented ? 1 : 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "silent") {
            FeatureFailure unused;
            return unused.status() == Status::ok ? 0 : 1;
        }
        check_catalog();
        check_context();
        check_errors();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

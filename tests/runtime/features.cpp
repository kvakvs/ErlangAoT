#include <cstdio>
#include <erlang_aot/abi/v1.hpp>
#include <erlang_aot/runtime/features.hpp>
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

// Render full/partial/escaped context through the actual stderr sink, retaining the first failure.
Status contextual_service(std::string_view mode) noexcept {
    FeatureFailure failure;
    if (mode == "context") {
        failure.report(FeatureId::garbage_collection, {"src/example.erl", 12, 5, "example", "native", "collect"});
    } else if (mode == "escaping") {
        failure.report(FeatureId::garbage_collection,
                       {"C:\\source\nfile.erl", 0, 99, "m\"\\\t", {}, std::string_view("x\0y", 3)});
    } else if (mode == "partial") {
        failure.report(FeatureId::garbage_collection, {"test.erl"});
    } else {
        failure.report(FeatureId::atom_collection);
    }
    return failure.report(FeatureId::allocation);
}

// Subprocess modes expose only stderr on failure and remain silent without a reached placeholder.
int run(int argc, char **argv) {
    if (argc != 2) {
        check_errors();
        return 0;
    }
    const std::string_view mode(argv[1]);
    if (mode == "stderr") {
        return unavailable_service() == Status::not_implemented ? 1 : 0;
    }
    if (mode == "silent") {
        FeatureFailure unused;
        return unused.status() == Status::ok ? 0 : 1;
    }
    return contextual_service(mode) == Status::not_implemented ? 1 : 0;
}

// Convert every host exception into a failing process result without throwing from main.
int main(int argc, char **argv) {
    try {
        return run(argc, argv);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    } catch (...) {
        std::fputs("unexpected feature reporting exception\n", stderr);
        return 1;
    }
}

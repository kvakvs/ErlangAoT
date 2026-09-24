#include "codegen/features.hpp"
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;
using namespace erlang_aot::codegen;
using namespace erlang_aot::abi::v1;

// Preserve assertions in release builds.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Each compiler/driver-owned entry fails once and invalidates previously staged artifacts.
void check_catalog() {
    for (const auto &feature : feature_catalog) {
        if (feature.owner == FeatureOwner::runtime) {
            continue;
        }
        CompilationResult result;
        require(result.add_output({"fixture", OutputKind::object, {std::byte{1}}}), "fixture staging failed");
        std::ostringstream errors;
        require(!reject_feature(result, feature.id, {}, errors), "placeholder returned success");
        require(!reject_feature(result, feature.id, {}, errors), "failure became successful on retry");
        require(result.status() == CompilationStatus::failed && result.outputs().empty(), "failure retained artifacts");
        require(!result.complete(), "failed batch completed");
        require(result.diagnostics().size() == 1, "duplicate owner report");
        require(errors.str() == '[' + std::string(feature.name) + "] notimpl\n", "unexpected compiler report");
    }
}

// Records own their strings/coordinates and mark delivery so later drivers cannot replay them.
void check_context() {
    CompilationResult result;
    std::ostringstream errors;
    std::string module = "example";
    require(!reject_feature(result, FeatureId::pattern_matching, {"src/example.erl", 12, 5, module, "native", "match"},
                            errors),
            "contextual failure returned success");
    module.clear();
    const auto records = result.diagnostics();
    const auto &report = records.front();
    require(report.reported && report.module_name == "example", "report state/context was borrowed");
    require(report.location && report.location->file == "src/example.erl" && report.location->line == 12 &&
                report.location->column == 5,
            "source coordinates were lost");
    require(errors.str() == report.message + '\n', "owner formatted message more than once");
    require(report.message.contains("[target=\"native\"] [operation=\"match\"]"), "target/operation missing");
}

// Prior ordinary errors and completed batches stay silent; invalid feature IDs are ordinary errors.
void check_nonfeatures() {
    CompilationResult failed;
    failed.report({DiagnosticLevel::error, "ordinary error", {}, {}});
    CompilationResult completed;
    require(completed.complete(), "fixture completion failed");
    std::ostringstream errors;
    reject_feature(failed, FeatureId::guards, {}, errors);
    reject_feature(completed, FeatureId::guards, {}, errors);
    require(errors.str().empty(), "inactive/failed pipeline emitted a placeholder");
    require(!failed.diagnostics().front().reported, "ordinary error incorrectly marked delivered");
    CompilationResult invalid;
    reject_feature(invalid, FeatureId::invalid, {}, errors);
    require(errors.str() == "invalid deferred feature ID 0\n", "invalid ID mislabeled notimpl");
}

// Both nonthrowing and throwing stream failures latch failure without exceptions or repeated output.
void check_sink_failure() {
    CompilationResult result;
    std::ostringstream errors;
    errors.setstate(std::ios::badbit);
    reject_feature(result, FeatureId::guards, {}, errors);
    require(result.diagnostic_capture_failed() && result.outputs().empty(), "stream failure was swallowed");
    CompilationResult throwing;
    std::ostringstream exceptions;
    exceptions.exceptions(std::ios::badbit);
    bool prepared = false;
    try {
        exceptions.setstate(std::ios::badbit);
    } catch (const std::ios_base::failure &) {
        // Confirm that the fixture throws before testing the reporter's containment.
        prepared = true;
    }
    require(prepared, "throwing stream fixture did not throw");
    reject_feature(throwing, FeatureId::guards, {}, exceptions);
    require(throwing.diagnostic_capture_failed(), "stream exception escaped or was swallowed");
}

// Subprocess modes prove default stderr delivery, propagation exit status and unused-reporter silence.
int main(int argc, char **argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "stderr") {
            CompilationResult result;
            reject_feature(result, FeatureId::pattern_matching, {"src/example.erl", 12, 5});
            reject_feature(result, FeatureId::pattern_matching);
            return result.status() == CompilationStatus::failed ? 1 : 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "silent") {
            CompilationResult unused;
            return unused.complete() ? 0 : 1;
        }
        check_catalog();
        check_context();
        check_nonfeatures();
        check_sink_failure();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}

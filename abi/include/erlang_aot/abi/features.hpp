#pragma once
#include <array>
#include <cstdint>
#include <string_view>

namespace erlang_aot::abi::v1 {
// Stable IDs are explicit and never recycled; names describe deferred semantics, not generic errors.
enum class FeatureId : std::uint8_t {
    // Reserve zero for a missing/unrecognized feature; it never has a catalog entry.
    invalid = 0,
    pattern_matching = 1,
    guards = 2,
    multiple_clauses = 3,
    arithmetic = 4,
    bignum_expressions = 5,
    atom_expressions = 6,
    heap_expressions = 7,
    dynamic_calls = 8,
    recursive_calls = 9,
    closures = 10,
    exceptions = 11,
    receive = 12,
    behavior_attributes = 13,
    term_services = 14,
    builtins = 15,
    process_execution = 16,
    message_passing = 17,
    scheduling = 18,
    allocation = 19,
    garbage_collection = 20,
    atom_collection = 21,
    dynamic_modules = 22,
    executable_linking = 23
};
enum class FeatureOwner : std::uint8_t { compiler, runtime, driver };
enum class FeatureStatus : std::uint8_t { deferred };

struct FeatureInfo {
    // Keep machine identity and diagnostic spelling stable across unrelated implementations.
    FeatureId id;
    std::string_view name;
    // Identify the sole reporter; callers propagate its failure without formatting it again.
    FeatureOwner owner;
    std::string_view boundary;
    // Record current availability and the plan step that installs the owning handler.
    FeatureStatus status;
    unsigned plan_step;
    // Name the focused CTest that exercises this entry's reporting contract.
    std::string_view failure_test;
};

// Catalog reservations do not install handlers or expand the executable language subset.
inline constexpr std::array feature_catalog{
    FeatureInfo{FeatureId::pattern_matching, "pattern matching", FeatureOwner::compiler, "capability/pattern analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::guards, "guards", FeatureOwner::compiler, "capability/guard analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::multiple_clauses, "multiple clauses", FeatureOwner::compiler, "capability/clause analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::arithmetic, "arithmetic", FeatureOwner::compiler, "expression lowering",
                FeatureStatus::deferred, 24, "codegen_features"},
    FeatureInfo{FeatureId::bignum_expressions, "bignum expressions", FeatureOwner::compiler,
                "integer capability analysis", FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::atom_expressions, "atom expressions", FeatureOwner::compiler, "literal capability analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::heap_expressions, "heap expressions", FeatureOwner::compiler,
                "aggregate capability analysis", FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::dynamic_calls, "dynamic calls", FeatureOwner::compiler, "call capability analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::recursive_calls, "recursive calls", FeatureOwner::compiler, "batch call graph analysis",
                FeatureStatus::deferred, 19, "codegen_features"},
    FeatureInfo{FeatureId::closures, "closures", FeatureOwner::compiler, "fun capability analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::exceptions, "exceptions", FeatureOwner::compiler, "exception capability analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::receive, "receive", FeatureOwner::compiler, "receive capability analysis",
                FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::behavior_attributes, "behavior-changing attributes", FeatureOwner::compiler,
                "attribute capability analysis", FeatureStatus::deferred, 16, "codegen_features"},
    FeatureInfo{FeatureId::term_services, "term services", FeatureOwner::runtime, "TermFactory constructors",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::builtins, "builtins", FeatureOwner::runtime, "native callable invocation",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::process_execution, "process execution", FeatureOwner::runtime, "SchedulerService::execute",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::message_passing, "message passing", FeatureOwner::runtime, "ProcessContext::send",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::scheduling, "scheduling", FeatureOwner::runtime, "SchedulerService::run",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::allocation, "allocation", FeatureOwner::runtime, "ProcessHeap::allocate",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::garbage_collection, "garbage collection", FeatureOwner::runtime, "ProcessHeap::collect",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::atom_collection, "atom collection", FeatureOwner::runtime, "AtomStorage::collect",
                FeatureStatus::deferred, 14, "runtime_features"},
    FeatureInfo{FeatureId::dynamic_modules, "dynamic modules", FeatureOwner::runtime, "CodeServer::unload",
                FeatureStatus::deferred, 14, "runtime_services"},
    FeatureInfo{FeatureId::executable_linking, "executable linking", FeatureOwner::driver,
                "driver final executable output", FeatureStatus::deferred, 35, "codegen_features"},
};

// Unknown IDs are invalid inputs, never an invented future feature or a successful fallback.
constexpr const FeatureInfo *find_feature(FeatureId id) noexcept {
    for (const auto &feature : feature_catalog) {
        if (feature.id == id) {
            return &feature;
        }
    }
    return nullptr;
}
} // namespace erlang_aot::abi::v1

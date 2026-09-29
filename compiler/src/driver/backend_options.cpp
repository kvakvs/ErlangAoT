#include "backend_options.hpp"
#include "../project/paths.hpp"
#include "options.hpp"
#include <map>

namespace erlang_aot::cli {
namespace {
// Give every value-bearing switch the same absent/empty operand diagnostic.
std::optional<std::string> operand(std::string_view option, std::span<char *> &remaining, std::string &value) {
    if (remaining.empty() || std::string_view(remaining.front()).empty()) {
        return "expected a value after " + std::string(option);
    }
    value = remaining.front();
    remaining = remaining.subspan(1);
    return {};
}

// Distinguish malformed artifact kinds from duplicate selections without choosing a fallback.
std::optional<std::string> emission(const std::string &value, BackendOptions &options) {
    if (options.emit) {
        return "--emit specified more than once";
    }
    static const std::map<std::string, codegen::OutputKind> kinds{{"obj", codegen::OutputKind::object},
                                                                  {"llvm-ir", codegen::OutputKind::llvm_ir},
                                                                  {"llvm-bc", codegen::OutputKind::llvm_bitcode}};
    const auto found = kinds.find(value);
    if (found == kinds.end()) {
        return "--emit expects obj, llvm-ir, or llvm-bc";
    }
    options.emit = found->second;
    return {};
}

// Assign native path/triple operands only once, preserving option spelling in errors.
std::optional<std::string> value_option(std::string_view option, const std::string &value, BackendOptions &options) {
    if (option == "--emit") {
        return emission(value, options);
    }
    if (option == "--artifact-dir") {
        if (options.artifact_directory) {
            return "--artifact-dir specified more than once";
        }
        options.artifact_directory = project::native_path(value);
        return {};
    }
    if (!options.target_triple.empty()) {
        return "--target-triple specified more than once";
    }
    options.target_triple = value;
    return {};
}

// Remember any explicit backend policy so frontend-only actions cannot silently discard it.
bool explicit_backend(const BackendOptions &options) {
    return options.emit || options.artifact_directory || !options.target_triple.empty() || options.optimization ||
           options.disable_type_specialization;
}
} // namespace

bool is_backend_option(std::string_view option) {
    return option == "--emit" || option == "--artifact-dir" || option == "--target-triple" || option == "-O0" ||
           option == "-O2" || option == "--no-type-specialization";
}

std::optional<std::string> parse_backend_option(std::string_view option, std::span<char *> &remaining,
                                                BackendOptions &options) {
    if (option == "--no-type-specialization") {
        options.disable_type_specialization = true;
        return {};
    }
    if (option == "-O0" || option == "-O2") {
        if (options.optimization) {
            return "optimization level specified more than once";
        }
        options.optimization = option == "-O2" ? codegen::OptimizationLevel::speed : codegen::OptimizationLevel::none;
        return {};
    }
    std::string value;
    if (const auto error = operand(option, remaining, value)) {
        return error;
    }
    return value_option(option, value, options);
}

std::optional<std::string> validate_backend_options(const Options &options) {
    if ((options.preprocess || options.project.create) && explicit_backend(options.backend)) {
        return "compilation switches cannot be combined with frontend actions or --new-project";
    }
    if (options.backend.emit && options.output_explicit) {
        return "--emit cannot be combined with --output";
    }
    if (options.backend.artifact_directory && !options.backend.emit) {
        return "--artifact-dir requires --emit";
    }
    return {};
}
} // namespace erlang_aot::cli

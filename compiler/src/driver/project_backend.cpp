#include "../artifacts/artifacts.hpp"
#include "../project/command.hpp"
#include "../project/paths.hpp"
#include "frontend.hpp"
#include "options.hpp"
#include "publication.hpp"
#include <iostream>

namespace clause::cli {
namespace {
// Resolve artifact roots independently of the target's executable output.
BackendOptions target_options(const Options &options, const project::Invocation &invocation,
                              const project::PlannedTarget &target) {
    auto backend = options.backend;
    const auto root = backend.artifact_directory
                          ? project::absolute_path(std::filesystem::current_path(), *backend.artifact_directory)
                          : invocation.file.parent_path() / "build" / "aot";
    backend.artifact_directory = root / artifacts::encoded_name(target.name);
    return backend;
}

// Protect every selected input and the manifest, including aliases into another target's batch.
std::vector<std::filesystem::path> protected_inputs(const project::Invocation &invocation) {
    std::vector<std::filesystem::path> inputs{invocation.file};
    for (const auto &target : invocation.targets) {
        inputs.insert(inputs.end(), target.sources.begin(), target.sources.end());
    }
    return inputs;
}

// Report a publication failure with the target that produced the output.
int publication_failure(const std::string &target, const std::string_view message) {
    std::cerr << "clau: error: target " << target << ": " << message << '\n';
    return 1;
}

// Compare destinations through existing links where possible, else lexically.
std::filesystem::path normalized(const std::filesystem::path &path) {
    std::error_code error;
    auto result = std::filesystem::weakly_canonical(path, error);
    return error ? path.lexically_normal() : result;
}

// Refuse two executables whose final names (after ".exe" normalization) are the same file.
const PendingExecutable *colliding(const std::vector<PendingExecutable> &pending, const PendingExecutable &current) {
    for (const auto &earlier : pending) {
        if (&earlier == &current) {
            return nullptr;
        }
        if (normalized(earlier.executable.output) == normalized(current.executable.output)) {
            return &earlier;
        }
    }
    return nullptr;
}

// Replace executable outputs only after every target linked and no two targets share an output.
int publish_executables(const std::vector<PendingExecutable> &pending) {
    for (const auto &item : pending) {
        if (const auto *earlier = colliding(pending, item)) {
            return publication_failure(item.project_target,
                                       "executable output " + project::path_text(item.executable.output) +
                                           " is also the output of target " + earlier->project_target);
        }
    }
    for (const auto &item : pending) {
        try {
            linking::publish_executable(item.executable);
        } catch (const std::exception &error) {
            return publication_failure(item.project_target, error.what());
        }
    }
    return 0;
}

// Defer all project publication until every selected target has compiled successfully.
int publish_targets(const std::vector<Publication> &pending) {
    for (const auto &batch : pending) {
        try {
            publish(batch);
        } catch (const std::exception &error) {
            return publication_failure(batch.project_target, error.what());
        }
    }
    return 0;
}
} // namespace

int run_project(const Options &options) {
    project::PlanOptions settings;
    settings.working_directory = std::filesystem::current_path();
    settings.preprocessing = options.preprocessing;
    settings.frontend = options.preprocess || options.backend.emit.has_value() || options.backend.inspect_ir() ||
                        options.backend.print_types;
    if (options.output_explicit) {
        settings.output = options.output;
    }
    settings.entry = options.entry;
    std::vector<Publication> pending;
    std::vector<PendingExecutable> executables;
    const project::TargetExecutor execute = [&](const project::Invocation &invocation,
                                                const project::PlannedTarget &target,
                                                const project::MessageSink &sink) {
        FrontendRequest request{options.print_pp,
                                options.print_ast,
                                options.parse_check,
                                !options.preprocess,
                                options.verbose,
                                target.preprocessing,
                                options.implementation_debug,
                                target_options(options, invocation, target)};
        request.project_target = target.name;
        request.print_source = options.print_source;
        request.executable_output = target.output;
        request.create_output_directory = !options.output_explicit;
        request.entry = target.entry;
        request.multiple_targets = invocation.targets.size() > 1;
        request.protected_inputs = protected_inputs(invocation);
        request.pending_publications = &pending;
        request.pending_executables = &executables;
        request.module_search_paths_ = target.search_paths_;
        return process_files(target.sources, request, sink);
    };
    const int status = project::run(options.project, settings, execute, std::cout, std::cerr);
    if (status != 0) {
        return status;
    }
    const int published = publish_targets(pending);
    return published == 0 ? publish_executables(executables) : published;
}
} // namespace clause::cli

#include "command.hpp"
#include "create.hpp"
#include "decode.hpp"
#include "diagnostics.hpp"
#include "paths.hpp"

namespace erlang_aot::project {
int run(const Request &request, const PlanOptions &options, const FileExecutor &executor, std::ostream &output,
        std::ostream &diagnostics) {
    try {
        if (request.create) {
            const auto path = create_project(options.working_directory, {*request.create});
            output << "Created " << path_text(path) << '\n';
            return 0;
        }
        if (!request.file) {
            fail({}, "missing --project request", 2);
        }
        const auto file = absolute_path(options.working_directory, *request.file);
        const auto manifest = decode(load(file));
        auto settings = options;
        settings.selectors = request.targets;
        const auto invocation = prepare(manifest, settings);
        return execute(invocation, executor,
                       [&](std::string_view message) { diagnostics << "erlangaot: " << message << '\n'; });
    } catch (const Failure &error) {
        diagnostics << "erlangaot: error: " << error.what() << '\n';
        return error.detail.exit_code;
    } catch (const std::exception &error) {
        diagnostics << "erlangaot: error: " << error.what() << '\n';
        return 1;
    }
}
} // namespace erlang_aot::project

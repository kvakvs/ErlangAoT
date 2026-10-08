#include "progress.hpp"
#include "../project/paths.hpp"
#include "display.hpp"
#include "frontend.hpp"
#include <iostream>

namespace clause::cli {
codegen::ProgressCallback progress_callback(const FrontendRequest &request) {
    if (!request.verbose) {
        return {};
    }
    return [target = request.project_target](const codegen::CompilationProgress &event) {
        std::cerr << "[comp] source=" << quote_text(project::path_text(event.source_path)) << " phase=" << event.phase
                  << " module=" << quote_text(event.module_name);
        if (!target.empty()) {
            std::cerr << " target=" << quote_text(target);
        }
        if (!event.detail.empty()) {
            std::cerr << " detail=" << quote_text(event.detail);
        }
        std::cerr << '\n';
    };
}
} // namespace clause::cli

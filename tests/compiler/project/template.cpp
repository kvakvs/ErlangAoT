#include "project/template.hpp"
#include "project/decode.hpp"
#include "support.hpp"
using namespace erlang_aot::project;

// Verify generated text remains deterministic, annotated, and fully schema-valid.
int main() {
    for (const bool windows : {false, true}) {
        const auto text = starter_template(windows);
        const auto manifest = decode(parse_document(text, "starter.toml"));
        require(manifest.targets.size() == 1);
        const auto &target = manifest.targets.front();
        require(target.name.value == "app");
        require(target.sources.empty());
        require(target.source_dirs.size() == 1 && target.source_dirs.front().value == "src");
        require(target.output && target.output->value == (windows ? "build/app.exe" : "build/app"));
        require(target.options.include_dirs.empty() && target.options.source_search_paths.empty());
        require(target.options.defines.empty() && target.options.applications.empty());
        require(target.options.enable_features.empty() && target.options.disable_features.empty());
        require(text == starter_template(windows));
        require(text.back() == '\n' && text.find('\r') == std::string::npos);
        for (const auto *annotation :
             {"--parse-check", "--target", "Erlang literal terms", "CLI -I", "include_lib", "Create/populate src"}) {
            require(text.find(annotation) != std::string::npos);
        }
    }
}

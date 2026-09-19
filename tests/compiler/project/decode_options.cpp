#include "project/decode.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
using namespace erlang_aot::project;

// Decode options through the public private-project entry point after TOML destruction.
Manifest parse(std::string_view options) {
    return decode(parse_document(
        std::string("schema_version=1\n[[targets]]\nname='app'\nsources=['main.erl']\n[targets.options]\n") +
            std::string(options),
        "options.toml"));
}

// Require complete nested schema validation before selecting any target.
void rejects(std::string_view input) {
    try {
        (void)parse(input);
    } catch (const Failure &error) {
        require(error.detail.site.target == "app");
        require(error.detail.site.line >= 5);
        return;
    }
    require(false);
}

// Check all option types, literal spelling, nested errors, and feature conflicts.
int main() {
    const auto manifest = parse(R"(source_search_paths=['src','generated']
include_dirs=['include']
defines=['FLAG','COUNT=2','LABEL="demo"']
enable_features=['maybe_expr']
disable_features=['compr_assign']
[targets.options.applications]
demo='vendor/demo'
)");
    const auto &options = manifest.targets.front().options;
    require(options.defines.size() == 3 && options.source_search_paths.size() == 2);
    require(options.enable_features.size() == 1 && options.disable_features.size() == 1);
    require(options.defines.back().value == "LABEL=\"demo\"");
    require(options.source_search_paths[1].value == "generated");
    require(options.applications.at("demo").value == "vendor/demo");
    require(options.enable_features.front().value == "maybe_expr");
    require(options.disable_features.front().value == "compr_assign");
    rejects("unknown=[]");
    rejects("defines=[true]");
    rejects("include_dirs='include'");
    rejects("enable_features=['x']\ndisable_features=['x']");
    rejects("applications=[]");
    rejects("[targets.options.applications]\ndemo=1");
    rejects("[targets.options.applications]\ndemo=''");
    rejects("[targets.options.applications]\n''='vendor'");
}

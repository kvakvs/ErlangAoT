#include "template.hpp"
#include <string_view>

namespace erlang_aot::project {
std::string starter_template(bool windows) {
    std::string result = R"PROJECT(# ErlangAoT project. Paths are relative to this TOML file's directory.
# Check with: erlangaot --parse-check --project <this-file.toml>
# Also available: --preprocess-check, --print-pp, and --print-ast.
schema_version = 1

# Add another [[targets]] block after this target's option tables for more targets.
# All targets run by default; select with --target app (repeat for more names).
[[targets]]
name = "app"

# List files or patterns, e.g. ["main.erl", "src/*.erl", "shared/**/*.erl"].
# Patterns support *, ?, and ** as a whole directory component.
sources = []
# Recursively collect .erl files. Create/populate src or edit these selections.
# Set source_dirs = [] when selecting files exclusively through sources.
source_dirs = ["src"]
# Future executable path; checks/printing do not write it. Windows: build/app.exe.
output = "@OUTPUT@"

[targets.options]
# Fallback roots for listed source filenames, e.g. ["src", "generated"].
# These do not discover extra files or search for headers.
source_search_paths = []
# Header search directories, first listed first, e.g. ["include", "vendor/include"].
# CLI -I directories take precedence; the last CLI -I is searched first.
include_dirs = []
# Predefined macros, e.g. ["DEBUG", "LIMIT=100", 'LABEL="demo"'].
# A bare name means true; values are Erlang literal terms. Duplicates are errors.
defines = []
# Empty lists preserve compiler feature defaults; CLI feature settings apply last.
# Example overrides: enable_features = ["maybe_expr"].
# Do not list a feature in both arrays.
enable_features = []
disable_features = []

# Explicit include_lib application roots. Empty by default.
# Example entry: my_dependency = "vendor/my_dependency"
[targets.options.applications]
)PROJECT";
    const auto position = result.find("@OUTPUT@");
    result.replace(position, std::string_view("@OUTPUT@").size(), windows ? "build/app.exe" : "build/app");
    return result;
}

std::string starter_template() {
#ifdef _WIN32
    return starter_template(true);
#else
    return starter_template(false);
#endif
}
} // namespace erlang_aot::project

#include "project/decode.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
using namespace erlang_aot::project;

// Verify rejected schema configurations keep filename and key diagnostics.
void rejects(std::string_view input, std::string_view expected, const Limits &limits = {}) {
    try {
        (void)decode(parse_document(input, "schema.toml"), limits);
    } catch (const Failure &error) {
        require(error.detail.site.file == "schema.toml");
        require(std::string_view(error.what()).find(expected) != std::string_view::npos);
        return;
    }
    require(false);
}

// Test exact schema types, target uniqueness, limits, and ownership after parsing.
int main() {
    const auto manifest =
        decode(parse_document("schema_version=1\n[[targets]]\nname='app'\nsources=['a.erl']", "owned.toml"));
    require(manifest.targets.front().name.value == "app");
    require(manifest.targets.front().sources.front().site.line == 4);
    rejects("targets=[]", "schema_version");
    rejects("schema_version=1.0\ntargets=[]", "schema_version");
    rejects("schema_version=2\ntargets=[]", "schema_version");
    rejects("schema_version=1\ntargets=[]", "nonempty");
    rejects("schema_version=1\ntargets=[3]", "expected a table");
    rejects("schema_version=1\nextra=1\ntargets=[]", "unknown key");
    const std::string prefix = "schema_version=1\n[[targets]]\n";
    rejects(prefix + "sources=['a.erl']", "missing target name");
    rejects(prefix + "name='-app'\nsources=['a.erl']", "invalid target name");
    rejects(prefix + "name='app'", "requires sources");
    rejects(prefix + "name='app'\nsources=[1]", "nonempty string");
    rejects(prefix + "name='app'\nsources=['']", "nonempty string");
    rejects(prefix + "name='app'\nsources='a.erl'", "array of strings");
    rejects(prefix + "name='app'\nsources=['a.erl']\nbogus=1", "unknown key");
    const std::string one = "name='app'\nsources=['a.erl']";
    rejects(prefix + one + "\n[[targets]]\n" + one, "duplicate target");
    Limits limits;
    limits.targets = 0;
    rejects(prefix + one, "target count limit", limits);
    limits.targets = 1;
    limits.entries = 1;
    rejects(prefix + one, "entry limit", limits);
}

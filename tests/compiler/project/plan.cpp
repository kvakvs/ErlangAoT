#include "project/plan.hpp"
#include "project/diagnostics.hpp"
#include "support.hpp"
#include <fstream>
using namespace erlang_aot::project;
namespace fs = std::filesystem;

// Confirm failures prevent publication of a partial invocation plan.
void rejects(const Manifest &manifest, const PlanOptions &options, int code = 1) {
    try {
        (void)prepare(manifest, options);
    } catch (const Failure &error) {
        require(error.detail.exit_code == code);
        return;
    }
    require(false);
}

// Exercise selection before discovery, output precedence, and alias collisions.
int main(int argc, char **argv) {
    require(argc == 2);
    const auto root = fs::absolute(argv[1]);
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream out(root / "main.erl");
        out << "-module(main).";
    }
    Target app;
    app.name = {"app", {}};
    app.sources = {{"main.erl", {}}};
    Target missing;
    missing.name = {"missing", {}};
    missing.sources = {{"absent.erl", {}}};
    Manifest manifest{root / "project.toml", {app, missing}};
    PlanOptions options;
    options.working_directory = root;
    options.selectors = {"app"};
    const auto selected = prepare(manifest, options);
    require(selected.targets.size() == 1);
    require(selected.targets.front().output->parent_path() == root / "build");
    require(!fs::exists(root / "build"));
    options.selectors.clear();
    rejects(manifest, options);
    manifest.targets[1] = app;
    manifest.targets[1].name.value = "tests";
    options.output = "requested";
    rejects(manifest, options, 2);
    options.selectors = {"app", "app"};
    require(prepare(manifest, options).targets.front().output == root / "requested");
    options.output.reset();
    options.selectors.clear();
    manifest.targets[0].output = Text{"out/../same", {}};
    manifest.targets[1].output = Text{"same", {}};
    rejects(manifest, options);
    options.frontend = true;
    require(prepare(manifest, options).targets.size() == 2);
    require(!prepare(manifest, options).targets.front().output);
    options.output = "ignored";
    rejects(manifest, options, 2);
    options.output.reset();
    options.frontend = false;
    {
        std::ofstream out(root / "existing");
        out << "preserve";
    }
    std::error_code error;
    fs::create_hard_link(root / "existing", root / "alias", error);
    if (!error) {
        manifest.targets[0].output = Text{"existing", {}};
        manifest.targets[1].output = Text{"alias", {}};
        rejects(manifest, options);
    }
    require(!fs::exists(root / "out") && !fs::exists(root / "requested"));
}

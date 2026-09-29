#include "artifacts/artifacts.hpp"
#include "project/paths.hpp"
#include "semantic/symbols.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace erlang_aot;
namespace fs = std::filesystem;

// Keep filesystem checks active in all configurations.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Read complete bytes to prove replacement and preservation of existing files.
std::string read(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), {}};
}

// Supply artifact bytes independently of LLVM for otherwise unreachable filesystem failure cases.
codegen::OutputBuffer output(std::string name) {
    return {std::move(name), codegen::OutputKind::llvm_ir, {std::byte{'O'}, std::byte{'K'}}};
}

// Every rejected batch must preserve existing files and clean its staging paths.
template <class Action> void rejects(Action action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, "invalid artifact publication succeeded");
}

// Exercise traversal-like identities, case differences, UTF-8 roots and complete-file replacement.
void valid(const fs::path &root) {
    const std::array outputs{output("../A"), output("../a"), output("\xc3\xa5")};
    artifacts::publish(outputs, root, {}, ".obj");
    artifacts::publish(outputs, root, {}, ".obj");
    for (const auto &item : outputs) {
        const auto name = artifacts::encoded_name(item.module_name);
        require(semantic::decode_symbol(name)->module == item.module_name, "filename is not reversible");
        require(read(root / (name + ".ll")) == "OK", "artifact bytes lost");
    }
}

// Preflight rejects duplicate destinations, hard-link inputs and nonregular outputs without replacing sentinels.
void invalid(const fs::path &root) {
    const auto path = root / (artifacts::encoded_name("preserve") + ".ll");
    {
        std::ofstream file(path);
        file << "sentinel";
    }
    const std::array duplicate{output("preserve"), output("preserve")};
    rejects([&] { artifacts::publish(duplicate, root, {}, ".obj"); });
    const std::array single{output("preserve")};
    const std::array inputs{path};
    rejects([&] { artifacts::publish(single, root, inputs, ".obj"); });
    const auto link = root / "input.erl";
    fs::create_hard_link(path, link);
    const std::array linked_inputs{link};
    rejects([&] { artifacts::publish(single, root, linked_inputs, ".obj"); });
    const std::array bad{output("preserve"), output("directory")};
    fs::create_directory(root / (artifacts::encoded_name("directory") + ".ll"));
    rejects([&] { artifacts::publish(bad, root, {}, ".obj"); });
    rejects([&] { artifacts::publish(single, path / "child", {}, ".obj"); });
    require(read(path) == "sentinel", "failed preflight changed an output");
}

// Overlong names fail cleanly and leave no private staging directory behind.
void failure(const fs::path &root) {
    const std::array outputs{output("preserve"), output(std::string(400, 'x'))};
    rejects([&] { artifacts::publish(outputs, root, {}, ".obj"); });
    require(read(root / (artifacts::encoded_name("preserve") + ".ll")) == "sentinel", "failed batch replaced output");
    for (const auto &entry : fs::directory_iterator(root)) {
        require(!entry.path().filename().string().starts_with(".erlangaot-stage-"), "staging directory leaked");
    }
}

// Use a CTest-owned directory; real CLI compilation assumes publication ownership in later steps.
int main(int argc, char **argv) {
    try {
        require(argc == 2, "expected test root");
        const auto root = project::native_path(argv[1]) / project::native_path("space \xc3\xa5");
        fs::remove_all(root);
        fs::create_directories(root);
        valid(root);
        invalid(root);
        failure(root);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

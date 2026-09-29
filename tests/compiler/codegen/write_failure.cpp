#include "artifacts/artifacts.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

namespace artifacts = erlang_aot::artifacts;
namespace cg = erlang_aot::codegen;

// Preserve complete output and remove every owned temporary after a partial write or close failure.
void check(const std::filesystem::path &root, const artifacts::PublicationIO &io) {
    std::filesystem::create_directories(root);
    const auto destination = root / (artifacts::encoded_name("sample") + ".ll");
    {
        std::ofstream file(destination);
        file << "preserved";
    }
    const std::array outputs{cg::OutputBuffer{"sample", cg::OutputKind::llvm_ir, {std::byte{42}, std::byte{43}}}};
    bool failed = false;
    try {
        artifacts::publish(outputs, root, {}, ".o", io);
    } catch (const std::runtime_error &) {
        failed = true;
    }
    std::ifstream input(destination);
    std::string content;
    input >> content;
    if (!failed || content != "preserved" || std::distance(std::filesystem::directory_iterator(root), {}) != 1) {
        throw std::runtime_error("partial artifact failure changed output or leaked staging files");
    }
}

// Deterministic hooks model faults ordinary CLI filesystems cannot request reliably.
int main(int argc, char **argv) {
    if (argc != 2) {
        return 2;
    }
    try {
        artifacts::PublicationIO write_failure;
        write_failure.write = [](auto &file, auto) {
            file << "partial";
            file.setstate(std::ios::badbit);
        };
        check(std::filesystem::path(argv[1]) / "write", write_failure);
        artifacts::PublicationIO interrupted;
        interrupted.write = [](auto &file, auto) {
            file << "partial";
            throw std::runtime_error("interrupted write");
        };
        check(std::filesystem::path(argv[1]) / "interrupted", interrupted);
        artifacts::PublicationIO close_failure;
        close_failure.close = [](auto &file) {
            file.close();
            file.setstate(std::ios::badbit);
        };
        check(std::filesystem::path(argv[1]) / "close", close_failure);
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

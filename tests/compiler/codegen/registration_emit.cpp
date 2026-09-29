#include "codegen/optimization.hpp"
#include "lowering_support.hpp"
#include <fstream>

// Emit real compiled modules for a separate, LLVM-free Clang consumer.
int main(int argc, char **argv) {
    try {
        require(argc == 3, "expected output directory");
        auto compilation =
            fixtures({"client.erl", "answer.erl"}, {},
                     std::string_view(argv[2]) == "O2" ? cg::OptimizationLevel::speed : cg::OptimizationLevel::none);
        require(analyze_and_lower(compilation) && cg::optimize(compilation) && cg::emit_objects(compilation),
                "registration emission failed");
        std::size_t index = 0;
        for (const auto &output : compilation.result().outputs()) {
            std::ofstream file(std::filesystem::path(argv[1]) / (std::to_string(index++) + ".obj"), std::ios::binary);
            file.write(reinterpret_cast<const char *>(output.bytes.data()),
                       static_cast<std::streamsize>(output.bytes.size()));
            file.close();
            require(!file.fail(), "object write failed");
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

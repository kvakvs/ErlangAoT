#include "codegen/optimization.hpp"
#include "lowering_support.hpp"
#include <fstream>

// Rename only the checked service declaration; source-generated selection/error continuations stay intact.
void seam(llvm::Module &module) {
    for (auto &function : module) {
        if (function.isDeclaration() && function.getName().contains("erlang_aot_bits_v1")) {
            function.setName("step16_bits");
        }
        if (function.isDeclaration() && function.getName().contains("erlang_aot_map_v1")) {
            function.setName("step15_map");
        }
        if (function.isDeclaration() && function.getName().contains("erlang_aot_immediate_v1")) {
            function.setName("step7_service");
        }
        if (function.isDeclaration() && function.getName().contains("erlang_aot_roots_enter_v5")) {
            function.setName("step11_roots");
        }
        if (function.isDeclaration() && function.getName().contains("erlang_aot_construct_v1")) {
            function.setName("step12_construct");
        }
        if (function.isDeclaration() && function.getName().contains("erlang_aot_inspect_v1")) {
            function.setName("step12_inspect");
        }
    }
}

// Emit labeled service-fault objects independently of the unmodified public-CLI/OTP kernels.
int main(int argc, char **argv) {
    try {
        require(argc == 3, "expected output and optimization mode");
        const std::string_view mode(argv[2]);
        auto compilation = fixtures({"service_client.erl", "service_answer.erl"}, {},
                                    mode.starts_with("O2") ? cg::OptimizationLevel::speed : cg::OptimizationLevel::none,
                                    !mode.ends_with("off"));
        require(analyze_and_lower(compilation), "service source lowering failed");
        for (auto &module : cg::detail::state(compilation).modules) {
            seam(*module);
        }
        require(cg::optimize(compilation) && cg::emit_objects(compilation), "service fault emission failed");
        std::size_t index = 0;
        for (const auto &output : compilation.result().outputs()) {
            std::ofstream file(std::filesystem::path(argv[1]) / (std::to_string(index++) + ".obj"), std::ios::binary);
            file.write(reinterpret_cast<const char *>(output.bytes.data()),
                       static_cast<std::streamsize>(output.bytes.size()));
            file.close();
            require(!file.fail(), "service object write failed");
        }
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

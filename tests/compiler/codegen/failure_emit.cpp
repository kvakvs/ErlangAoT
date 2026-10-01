#include "codegen/optimization.hpp"
#include "lowering_support.hpp"
#include <fstream>
#include <llvm/IR/IRBuilder.h>

// Replace only leaf operations with a native fault seam; retain real source local/remote call lowering.
void inject(llvm::Module &module, std::string_view function, std::string_view service) {
    auto *entry = module.getFunction(
        semantic::encode_symbol({"failure_answer", std::string(function), function == "take" ? 2U : 0U}));
    require(entry != nullptr, "missing fault leaf");
    entry->deleteBody();
    llvm::IRBuilder<> builder(llvm::BasicBlock::Create(module.getContext(), "entry", entry));
    auto target = module.getOrInsertFunction(service, entry->getFunctionType());
    builder.CreateRet(builder.CreateCall(target, {entry->getArg(0), entry->getArg(1)}));
}

// Emit fault-instrumented objects for separate native execution, never claim source error semantics yet.
int main(int argc, char **argv) {
    try {
        require(argc == 3, "expected output directory and optimization");
        const std::string_view mode(argv[2]);
        auto compilation = fixtures({"failure_client.erl", "failure_answer.erl"}, {},
                                    mode.starts_with("O2") ? cg::OptimizationLevel::speed : cg::OptimizationLevel::none,
                                    !mode.ends_with("off"));
        require(analyze_and_lower(compilation), "failure source lowering failed");
        auto &answer = *cg::detail::state(compilation).modules[1];
        inject(answer, "leaf", "step2_leaf");
        inject(answer, "later", "step2_later");
        inject(answer, "take", "step2_take");
        require(cg::optimize(compilation) && cg::emit_objects(compilation), "failure emission failed");
        std::size_t index = 0;
        for (const auto &output : compilation.result().outputs()) {
            std::ofstream file(std::filesystem::path(argv[1]) / (std::to_string(index++) + ".obj"), std::ios::binary);
            file.write(reinterpret_cast<const char *>(output.bytes.data()),
                       static_cast<std::streamsize>(output.bytes.size()));
            file.close();
            require(!file.fail(), "failure object write failed");
        }
    } catch (const std::exception &error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}

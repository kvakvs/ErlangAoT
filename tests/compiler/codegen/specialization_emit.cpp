#include "codegen/integer_guards.hpp"
#include "codegen/specialization_lowering.hpp"
#include "codegen/verification.hpp"
#include "lowering_support.hpp"
#include <fstream>
#include <llvm/Transforms/Utils/Cloning.h>

// Model implemented tag checks that current Erlang source syntax cannot yet express.
void checked_body(llvm::Function &function, unsigned repetitions) {
    function.deleteBody();
    auto &context = function.getContext();
    auto *word = llvm::cast<llvm::IntegerType>(function.getReturnType());
    llvm::IRBuilder<> builder(llvm::BasicBlock::Create(context, "entry", &function));
    auto *slot = builder.CreateGEP(word, function.getArg(1), llvm::ConstantInt::get(word, 0));
    auto *value = builder.CreateAlignedLoad(word, slot, llvm::Align(word->getBitWidth() / 8));
    llvm::Value *condition = builder.getTrue();
    for (unsigned i = 0; i < repetitions; ++i) {
        auto *mask = builder.CreateAnd(value, llvm::ConstantInt::get(word, 15));
        auto *check = builder.CreateICmpEQ(mask, llvm::ConstantInt::get(word, 15));
        condition = builder.CreateAnd(condition, check);
    }
    auto *hit = llvm::BasicBlock::Create(context, "hit", &function);
    auto *miss = llvm::BasicBlock::Create(context, "miss", &function);
    builder.CreateCondBr(condition, hit, miss);
    builder.SetInsertPoint(hit);
    builder.CreateRet(value);
    builder.SetInsertPoint(miss);
    slot = builder.CreateGEP(word, function.getArg(1), llvm::ConstantInt::get(word, 1));
    builder.CreateRet(builder.CreateAlignedLoad(word, slot, llvm::Align(word->getBitWidth() / 8)));
}

// Verify measured IR limits, whole-draft rollback and actual context forwarding before native emission.
void specialize(cg::Compilation &compilation) {
    auto &module = *cg::detail::state(compilation).modules.front();
    const auto symbol = semantic::encode_symbol({"guards", "select", 2});
    auto &function = *module.getFunction(symbol);
    checked_body(function, 16);
    require(cg::integer_guards(function, 2).size() == 16, "guard recognition failed");
    llvm::ValueToValueMapTy mapping;
    auto *reference = llvm::CloneFunction(&function, mapping);
    reference->setName(symbol + ".reference");
    reference->setLinkage(llvm::GlobalValue::ExternalLinkage);
    const auto baseline = function.getInstructionCount();
    cg::CompilationRequest speed;
    speed.optimization = cg::OptimizationLevel::speed;
    const cg::SpecializationInput input{
        "guards", symbol, baseline, {16, 0}, {{cg::Representation::small_integer, cg::Representation::generic}}};
    auto plan = cg::plan_specializations(speed, std::span(&input, 1));
    require(plan.candidates.size() == 1, "useful candidate was not planned");
    const auto budgeted_symbol = semantic::encode_symbol({"guards", "budgeted", 2});
    auto *oversize = module.getFunction(budgeted_symbol);
    checked_body(*oversize, 1);
    const auto oversized_baseline = oversize->getInstructionCount();
    // Deliberately stale estimate proves that emission independently measures and rolls back actual growth.
    plan.candidates.push_back({"guards", budgeted_symbol, input.profiles.front(), 1});
    cg::lower_specializations(module, plan);
    require(plan.lowered_variants == 1 && plan.rejected_variants == 1, "actual growth limit not enforced");
    require(oversize->getInstructionCount() == oversized_baseline && !module.getFunction(budgeted_symbol + ".type"),
            "over-budget draft changed generic IR");
    std::size_t added = module.getFunction(symbol)->getInstructionCount();
    added += module.getFunction(symbol + ".type")->getInstructionCount();
    require(added <= baseline, "pre-optimization function grew beyond 2x");
    for (const auto &use : module.getFunction(symbol)->getArg(0)->uses()) {
        require(use.getOperandNo() == 0, "dispatch lost context position");
    }
    require(cg::verify_ir(compilation), "specialized IR verification failed");
}

// Keep source/spec analysis generic, then exercise the narrower LLVM-stage invariant explicitly.
cg::Compilation exercise(const std::string &triple = {}) {
    auto compilation = fixtures({"guards.erl", "answer.erl"}, triple, cg::OptimizationLevel::speed);
    require(analyze_and_lower(compilation), "source lowering failed");
    require(cg::detail::state(compilation).specializations.candidates.empty(), "specification invented a benefit");
    specialize(compilation);
    require(cg::emit_objects(compilation), "specialized object emission failed");
    return compilation;
}

// Hand native objects to an independent runtime-only consumer; also verify 32-bit guard IR where available.
int main(int argc, char **argv) {
    try {
        require(argc == 2, "expected output directory");
#ifdef ERLANG_AOT_LLVM_X86
        (void)exercise("i686-unknown-linux-gnu");
#endif
        const auto compilation = exercise();
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

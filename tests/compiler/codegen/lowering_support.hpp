#pragma once
#include "codegen/emission.hpp"
#include "codegen/llvm_state.hpp"
#include "codegen/lowering.hpp"
#include "semantic/bindings.hpp"
#include "semantic/capabilities.hpp"
#include "semantic/symbols.hpp"
#include "semantic/types/contracts.hpp"
#include <algorithm>
#include <cstdio>
#include <erlang_aot/compiler/parser.hpp>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Object/ObjectFile.h>
#include <llvm/Support/Error.h>
#include <stdexcept>

using namespace erlang_aot;
namespace cg = erlang_aot::codegen;

// Exercise real source until the artifact-producing CLI replaces this stage adapter.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Own parsed fixture syntax before borrowing any semantic declaration addresses.
cg::Compilation fixtures(std::initializer_list<const char *> names, const std::string &triple = {},
                         cg::OptimizationLevel optimization = cg::OptimizationLevel::none, bool specialize = true) {
    cg::CompilationRequest request;
    request.target_triple = triple;
    request.optimization = optimization;
    request.disable_type_specialization = !specialize;
    for (const auto *name : names) {
        SourceManager sources;
        const auto path = std::filesystem::path(LOWERING_FIXTURES) / name;
        PreprocessorSession pp(sources.read(path));
        auto parsed = parse_module(pp);
        require(!parsed.failed, "fixture parse failed");
        request.inputs.emplace_back(path, std::move(parsed.module));
    }
    return cg::Compilation(std::move(request));
}

// Preserve the single-module adapter for earlier literal and parameter cases.
cg::Compilation fixture(const char *name, const std::string &triple = {}) { return fixtures({name}, triple); }

// Run existing declaration, capability, binding, call and type stages without replacement mocks.
bool analyze_and_lower(cg::Compilation &compilation, semantic::types::Limits inference_limits = {}) {
    const semantic::Reporter report = [](const Diagnostic &diagnostic) {
        require(diagnostic.severity != Severity::error, "fixture semantics failed");
    };
    std::vector<std::unique_ptr<semantic::Module>> modules;
    for (const auto &input : compilation.request().inputs) {
        auto module = semantic::index(input.syntax, "fixture.erl", report);
        semantic::check_capabilities(*module, report, 64);
        semantic::bind_parameters(*module, report);
        modules.push_back(std::move(module));
    }
    const auto calls = semantic::resolve_calls(modules, report);
    const auto declared = semantic::types::resolve_declarations(modules, report);
    const auto inferred = semantic::types::infer(calls, inference_limits);
    semantic::types::check_contracts(*declared, *inferred, calls, report);
    return cg::lower(compilation, modules, *inferred);
}

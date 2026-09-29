#include "limits.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace erlang_aot::codegen {
namespace {
// Subtract before accumulating so even injected size_t ceilings cannot overflow.
void consume(std::size_t size, std::size_t &remaining, const char *message) {
    if (size > remaining) {
        throw std::length_error(message);
    }
    remaining -= size;
}

// Count every owned syntax category, including declaration metadata and unused bodies.
void charge(const ast::Module &module, const CompilationLimits &limits, std::size_t &remaining) {
    auto local = limits.module_nodes;
    for (const auto count : std::array{module.forms().size(), module.expression_count(), module.pattern_count(),
                                       module.term_count(), module.type_count()}) {
        consume(count, local, "compilation module syntax limit exceeded");
        consume(count, remaining, "compilation batch syntax limit exceeded");
    }
}
} // namespace

void validate_input_limits(std::span<const CompilationInput> inputs, const CompilationLimits &limits,
                           const ast::Module *additional) {
    if (inputs.size() > limits.modules || (additional && inputs.size() == limits.modules)) {
        throw std::length_error("compilation module count limit exceeded");
    }
    auto remaining = limits.batch_nodes;
    for (const auto &input : inputs) {
        charge(input.syntax, limits, remaining);
    }
    if (additional) {
        charge(*additional, limits, remaining);
    }
}

std::size_t output_capacity(const CompilationLimits &limits, std::span<const OutputBuffer> outputs) {
    auto remaining = limits.batch_bytes;
    for (const auto &output : outputs) {
        consume(output.bytes.size(), remaining, "compilation artifact batch limit exceeded");
    }
    return std::min(limits.module_bytes, remaining);
}
} // namespace erlang_aot::codegen

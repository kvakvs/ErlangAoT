#include "../features.hpp"
#include "contracts.hpp"

namespace clause::semantic::types {
namespace {
// Quote control bytes and delimiters so filenames and atoms cannot inject debug lines.
std::string escaped(const std::string_view text) {
    constexpr std::string_view hex = "0123456789abcdef";
    std::string result;
    result.reserve(text.size() + 2);
    result += '"';
    for (const unsigned char character : text) {
        if (character < 32 || character == 127 || character == '"' || character == '\\') {
            result += "\\x";
            result += hex[character >> 4];
            result += hex[character & 15];
        } else {
            result += static_cast<char>(character);
        }
    }
    return result + '"';
}

// Render relations explicitly instead of displaying an unknown parameter as a concrete type.
std::string fact_text(const Inference &inferred, const Fact fact) {
    if (fact.argument) {
        return "argument[" + std::to_string(*fact.argument) + "]";
    }
    const auto &node = inferred.graph.get(fact.type);
    return node.kind == Kind::integer ? node.name : "term()";
}

// Preserve exact arity even for zero-argument functions; integer singleton inputs show their value.
std::string input_text(const Inference &inferred, const Summary &summary) {
    std::string result = "[";
    for (std::size_t i = 0; i < summary.inputs.size(); ++i) {
        result += i == 0 ? "" : ",";
        result += fact_text(inferred, {summary.inputs[i]});
    }
    return result + ']';
}
} // namespace

void trace_inference(const Inference &inferred, const CallGraph &calls, const DiagnosticSink &sink, const int step) {
    for (const auto function : calls.order) {
        const auto &summary = inferred.functions.at(function.function);
        sink("[impldebug " + std::to_string(step) + "] " + escaped(function.module->file) + " inference " +
             escaped(utf8(function.module->name)) + ":" + escaped(utf8(function.function->key.name)) + "/" +
             std::to_string(function.function->key.arity) + " inputs=" + input_text(inferred, summary) +
             " result=" + fact_text(inferred, summary.result));
    }
    if (inferred.graph.widened()) {
        sink("[impldebug " + std::to_string(step) + "] inference budget exhausted; widened to term()");
    }
}
} // namespace clause::semantic::types

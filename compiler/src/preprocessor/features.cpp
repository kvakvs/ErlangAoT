#include "engine.hpp"
#include <algorithm>

namespace erlang_aot {
namespace {
// OTP's query macros expand to ordinary comparisons, preserving arbitrary argument syntax.
Definition query_definition(std::string_view name, const std::vector<std::string> &features) {
    std::string body = "false";
    if (!features.empty()) {
        body = "(";
        for (auto it = features.rbegin(); it != features.rend(); ++it) {
            if (it != features.rbegin()) {
                body += " orelse ";
            }
            body += "(X) == " + *it;
        }
        body += ')';
    }
    auto parsed = parse_directive(fragment("-define(" + std::string(name) + "(X)," + body + ")."));
    return std::get<Definition>(std::get<Directive>(std::move(parsed)).operand);
}
} // namespace

void PreprocessorSession::State::feature_macros() {
    macros.define(query_definition("FEATURE_AVAILABLE", {"compr_assign", "maybe_expr"}));
    macros.define(query_definition("FEATURE_ENABLED", enabled_features));
}

void PreprocessorSession::State::feature(const std::string &name, bool enabled, const Token &site) {
    if (!prefix) {
        pp_fail(DiagnosticCode::invalid_feature, "feature directive appears after module prefix", site);
    }
    if (!features.contains(name)) {
        pp_fail(DiagnosticCode::invalid_feature, "unknown OTP 29.1 feature: " + name, site);
    }
    auto &setting = features.at(name);
    const auto forbidden = enabled ? FeatureLifecycle::rejected : FeatureLifecycle::permanent;
    if (setting.lifecycle == forbidden) {
        pp_fail(DiagnosticCode::invalid_feature, "feature lifecycle forbids this configuration", site);
    }
    setting.enabled = enabled;
    if (!enabled) {
        std::erase(enabled_features, name);
    } else if (std::ranges::find(enabled_features, name) == enabled_features.end()) {
        enabled_features.insert(enabled_features.begin(), name);
    }
    macros.definitions.insert_or_assign({U"FEATURE_ENABLED", 1}, query_definition("FEATURE_ENABLED", enabled_features));
}

void PreprocessorSession::State::keywords() {
    auto &lexer = files.back().lexer;
    lexer.set_keyword(U"maybe", features.at("maybe_expr").enabled);
    lexer.set_keyword(U"else", features.at("maybe_expr").enabled);
}
} // namespace erlang_aot

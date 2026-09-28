#include "expression_capability.hpp"
#include "features.hpp"
#include <algorithm>
#include <array>

namespace erlang_aot::semantic {
namespace {
// An explicit allowlist keeps executable or unknown attributes from silently changing semantics.
struct FormCapability {
    std::string_view operator()(const ast::ModuleAttribute &value) const {
        return value.parameters ? "behavior-changing attributes" : "";
    }

    std::string_view operator()(const ast::FileAttribute &) const { return {}; }

    std::string_view operator()(const ast::Function &) const { return {}; }

    std::string_view operator()(const ast::ExportAttribute &) const { return {}; }

    std::string_view operator()(const ast::TypeDeclaration &) const { return {}; }

    std::string_view operator()(const ast::Specification &) const { return {}; }

    std::string_view operator()(const ast::DocumentationAttribute &) const { return {}; }

    std::string_view operator()(const ast::ImportAttribute &) const { return "behavior-changing attributes"; }

    std::string_view operator()(const ast::ImportRecordAttribute &) const { return "behavior-changing attributes"; }

    std::string_view operator()(const ast::RecordDeclaration &) const { return {}; }

    std::string_view operator()(const ast::GenericAttribute &value) const {
        constexpr std::array<std::u32string_view, 6> allowed{U"author",     U"vsn",         U"copyright",
                                                             U"deprecated", U"export_type", U"optional_callbacks"};
        return std::ranges::contains(allowed, value.name.name) ? "" : "behavior-changing attributes";
    }
};

// Use one located diagnostic path for all capability rejection sites.
void unsupported(const Module &module, const ast::NodeSource &source, std::string_view reason, const Reporter &out) {
    reject_capability(module, source, reason, out);
}

// Inspect every executable child iteratively, including unused functions and nested call arguments.
void body(const Module &module, const ast::FunctionClause &clause, const Reporter &out, const unsigned bits) {
    if (clause.body.size() != 1) {
        unsupported(module, clause.source, "expression sequences", out);
    }
    auto pending = clause.body;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        const auto &expression = module.syntax->expression(id);
        const auto reason = std::visit(ExpressionCapability{*module.syntax, id, bits}, expression.value);
        if (!reason.empty()) {
            unsupported(module, expression.source, reason, out);
            continue;
        }
        const auto children = expression_children(expression);
        pending.insert(pending.end(), children.rbegin(), children.rend());
    }
}

// Parameter patterns and guards remain separate from expression capability decisions.
void head(const Module &module, const ast::FunctionClause &clause, const Reporter &out) {
    if (clause.guard) {
        unsupported(module, clause.guard->source, "guards", out);
    }
    for (const auto &id : clause.arguments) {
        const auto &pattern = module.syntax->pattern(id);
        const auto expression = std::visit([](const auto &value) { return value.expression; }, pattern.value);
        if (!std::holds_alternative<ast::Variable>(
                module.syntax->expression(ungroup(*module.syntax, expression)).value)) {
            unsupported(module, pattern.source, "pattern matching", out);
        }
    }
}

// Check all clauses even when the clause count itself exceeds the milestone.
void function(const Module &module, const ast::Function &value, const ast::NodeSource &source, const Reporter &out,
              const unsigned bits) {
    if (value.clauses.size() != 1) {
        unsupported(module, source, "multiple clauses", out);
    }
    for (const auto &clause : value.clauses) {
        head(module, clause, out);
        body(module, clause, out, bits);
    }
}
} // namespace

void check_capabilities(const Module &module, const Reporter &out, const unsigned word_bits) {
    for (const auto &id : module.syntax->forms()) {
        const auto &form = module.syntax->form(id);
        const auto reason = std::visit(FormCapability{}, form.value);
        if (!reason.empty()) {
            unsupported(module, form.source, reason, out);
        }
        if (const auto *value = std::get_if<ast::Function>(&form.value)) {
            function(module, *value, form.source, out, word_bits);
        }
    }
}
} // namespace erlang_aot::semantic

#include "tree.hpp"
#include "token_text.hpp"
#include <algorithm>
#include <erlang_aot/compiler/printing.hpp>

namespace erlang_aot::printing {
std::string literal(TokenKind kind, const TokenValue &value) { return utf8(token_text(kind, value)); }

std::string atom(const ast::Atom &value) { return literal(TokenKind::atom, value.name); }

TreePrinter::TreePrinter(std::ostream &output, const ast::Module &module, std::size_t visits)
    : output_(output), module_(module), visits_(visits) {}

void TreePrinter::child(std::string role, Reference reference) {
    if (visits_ == 0)
        throw std::length_error("AST printing visit budget exhausted");
    --visits_;
    pending_.push_back({std::move(role), depth_ + 1, std::move(reference)});
}

void TreePrinter::optional_child(std::string role, const std::optional<ast::ExprId> &reference) {
    if (reference) {
        child(std::move(role), *reference);
    }
}

void TreePrinter::prefix(const Work &work) {
    constexpr std::size_t indentation_limit = 64;
    output_ << std::string(std::min(work.depth, indentation_limit) * 2, ' ');
    if (work.depth > indentation_limit) {
        output_ << "[depth=" << work.depth << "] ";
    }
    output_ << work.role << ": ";
}

void TreePrinter::visit(const ast::FormId &id) { module_.visit(id, *this); }

void TreePrinter::visit(const ast::ExprId &id) { module_.visit(id, *this); }

void TreePrinter::visit(const ast::PatternSyntaxId &id) { module_.visit(id, *this); }

void TreePrinter::run() {
    output_ << "Module forms=" << module_.forms().size() << '\n';
    handles("form", module_.forms());
    std::ranges::reverse(pending_);
    while (!pending_.empty()) {
        auto work = std::move(pending_.back());
        pending_.pop_back();
        depth_ = work.depth;
        const auto start = pending_.size();
        prefix(work);
        std::visit([this](const auto &reference) { visit(reference); }, work.reference);
        output_ << '\n';
        std::ranges::reverse(std::span(pending_).subspan(start));
    }
}
} // namespace erlang_aot::printing

namespace erlang_aot {
void print_ast(std::ostream &output, const ast::Module &module, std::size_t visits) {
    printing::TreePrinter(output, module, visits).run();
}
} // namespace erlang_aot

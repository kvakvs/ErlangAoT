#include "tree.hpp"
#include "token_text.hpp"
#include <algorithm>
#include <erlang_aot/compiler/printing.hpp>

namespace erlang_aot::printing {
std::string literal(const TokenKind kind, const TokenValue &value) { return utf8(token_text(kind, value)); }

std::string atom(const ast::Atom &value) { return literal(TokenKind::atom, value.name); }

TreePrinter::TreePrinter(std::ostream &output, const ast::Module &module, const std::size_t visits)
    : output_(output), module_(module), visits_(visits) {}

void TreePrinter::child(std::string role, Reference reference) {
    if (visits_ == 0) {
        throw std::length_error("AST printing visit budget exhausted");
    }
    --visits_;
    pending_.push_back({.role = std::move(role), .depth = depth_ + 1, .reference = std::move(reference)});
}

void TreePrinter::optional_child(std::string role, const std::optional<ast::ExprId> &reference) {
    if (reference) {
        child(std::move(role), *reference);
    }
}

void TreePrinter::indentation(const std::size_t depth) const {
    constexpr std::size_t indentation_limit = 64;
    output_ << std::string(std::min(depth, indentation_limit) * 2, ' ');
    if (depth > indentation_limit) {
        output_ << "[depth=" << depth << "] ";
    }
}

void TreePrinter::prefix(const Work &work) const {
    indentation(work.depth);
    output_ << work.role << "=(";
}

void TreePrinter::close_objects(const std::size_t next_depth) {
    while (!open_depths_.empty() && open_depths_.back() >= next_depth) {
        indentation(open_depths_.back());
        output_ << ")\n";
        open_depths_.pop_back();
    }
}

void TreePrinter::visit(const ast::FormId &id) { module_.visit(id, *this); }

void TreePrinter::visit(const ast::ExprId &id) { module_.visit(id, *this); }

void TreePrinter::visit(const ast::PatternSyntaxId &id) { module_.visit(id, *this); }

void TreePrinter::run() {
    output_ << "(Module forms=" << module_.forms().size();
    if (module_.forms().empty()) {
        output_ << ")\n";
        return;
    }
    output_ << '\n';
    open_depths_.push_back(0);
    handles("form", module_.forms());
    std::ranges::reverse(pending_);
    while (!pending_.empty()) {
        auto work = std::move(pending_.back());
        pending_.pop_back();
        depth_ = work.depth;
        close_objects(depth_);
        const auto start = pending_.size();
        prefix(work);
        std::visit([this](const auto &reference) { visit(reference); }, work.reference);
        if (pending_.size() == start) {
            output_ << ')';
        } else {
            open_depths_.push_back(depth_);
        }
        output_ << '\n';
        std::ranges::reverse(std::span(pending_).subspan(start));
    }
    close_objects(0);
}
} // namespace erlang_aot::printing

namespace erlang_aot {
void print_ast(std::ostream &output, const ast::Module &module, const std::size_t visits) {
    printing::TreePrinter(output, module, visits).run();
}
} // namespace erlang_aot

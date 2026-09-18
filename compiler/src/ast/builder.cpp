#include "builder.hpp"
#include "storage.hpp"

namespace erlang_aot::ast {
namespace {
// Copy source metadata without retaining decoded token values in every node.
TokenOrigin origin(const Token &token) { return {token.spelling, token.location, token.origins}; }

// Empty forms retain their explicitly supplied EOF anchor independently of tokens.
detail::OriginTable origin_table(std::span<const Token> tokens, const Token &end, FeatureSnapshot features) {
    detail::OriginTable table{{}, origin(end), std::move(features)};
    table.tokens.reserve(tokens.size());
    for (const auto &token : tokens) {
        table.tokens.push_back(origin(token));
    }
    return table;
}
} // namespace

Builder::Transaction::Transaction(Builder &builder, std::span<const Token> tokens, const Token &end,
                                  FeatureSnapshot features)
    : builder_(builder), expressions_(builder.module_.storage().expressions.size()),
      forms_(builder.module_.storage().forms.size()), patterns_(builder.module_.storage().patterns.size()),
      origins_(builder.module_.storage().origins.size()) {
    if (builder.active_) {
        throw std::logic_error("nested AST form transaction");
    }
    builder.active_ = builder.module_.storage_->origins.append(origin_table(tokens, end, std::move(features)));
}

Builder::Transaction::~Transaction() {
    if (!committed_) {
        auto &storage = *builder_.module_.storage_;
        storage.forms.truncate(forms_);
        storage.patterns.truncate(patterns_);
        storage.expressions.truncate(expressions_);
        storage.origins.truncate(origins_);
        builder_.active_.reset();
    }
}

void Builder::Transaction::commit(FormId root) {
    if (committed_ || !builder_.active_) {
        throw std::logic_error("closed AST form transaction");
    }
    auto &storage = *builder_.module_.storage_;
    builder_.validate(storage.forms.get(root).source);
    if (storage.forms.size() != forms_ + 1) {
        throw std::logic_error("transaction must publish exactly one form");
    }
    storage.roots.push_back(std::move(root));
    committed_ = true;
    builder_.active_.reset();
}

Builder::Transaction Builder::begin(std::span<const Token> tokens, const Token &end, FeatureSnapshot features) {
    return Transaction(*this, tokens, end, std::move(features));
}

NodeSource Builder::source(std::size_t begin, std::size_t end, std::size_t anchor) const {
    if (!active_) {
        throw std::logic_error("AST source requires a form transaction");
    }
    NodeSource result{*active_, begin, end, anchor};
    validate(result);
    return result;
}

void Builder::validate(const NodeSource &source) const {
    if (!active_ || source.form != *active_) {
        throw std::invalid_argument("AST node belongs to another form");
    }
    detail::source_table(module_.storage(), source);
}

ExprId Builder::expression(ExprValue value, NodeSource source) {
    validate(source);
    validate(value);
    return module_.storage_->expressions.append({std::move(value), std::move(source)});
}

FormId Builder::form(FormValue value, NodeSource source) {
    validate(source);
    validate(value);
    return module_.storage_->forms.append({std::move(value), std::move(source)});
}

const Module &Builder::view() const { return module_; }

Module Builder::finish(FeatureSnapshot features) && {
    if (active_) {
        throw std::logic_error("cannot finish an active AST transaction");
    }
    module_.storage_->features = std::move(features);
    return std::move(module_);
}
} // namespace erlang_aot::ast

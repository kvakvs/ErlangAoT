#include "forms.hpp"
#include <erlang_aot/compiler/parser.hpp>

namespace erlang_aot {
struct ParserSession::State {
    // All work, results, and failure state belong to this one module.
    ParserLimits limits;
    ast::Builder builder;
    std::vector<Diagnostic> diagnostics;
    std::size_t tokens = 0;
    bool failed = false;
    bool stopped = false;
    // Consume one message, reserving one extra slot for diagnostic-budget exhaustion.
    void record(Diagnostic diagnostic);
    // Reject exhausted work/storage before retaining another form's origin table.
    void budget(std::size_t count, const Token &anchor);
    void parse(std::span<const Token> input, const Token &end, FeatureSnapshot features);
    // Route every event alternative explicitly; directives must not reach semantic parsing.
    void consume(const OrdinaryForm &form);
    void consume(const Diagnostic &diagnostic);
    void consume(const Directive &directive);
};

void ParserSession::State::record(Diagnostic diagnostic) {
    failed = failed || diagnostic.severity == Severity::error;
    if (stopped) {
        return;
    }
    if (diagnostics.size() >= limits.diagnostics) {
        diagnostic.code = DiagnosticCode::resource_limit;
        diagnostic.severity = Severity::error;
        diagnostic.message = "parser diagnostic budget exhausted";
        failed = true;
    }
    stopped = diagnostic.code == DiagnosticCode::resource_limit;
    diagnostics.push_back(std::move(diagnostic));
}

void ParserSession::State::budget(std::size_t count, const Token &anchor) {
    if (count > limits.form_tokens || count > limits.total_tokens - tokens) {
        throw token_diagnostic(DiagnosticCode::resource_limit, "parser token budget exhausted", anchor);
    }
    tokens += count;
}

void ParserSession::State::parse(std::span<const Token> input, const Token &end, FeatureSnapshot features) {
    if (stopped) {
        return;
    }
    try {
        budget(input.size(), input.empty() ? end : input.front());
        auto transaction = builder.begin(input, end, std::move(features));
        const auto used =
            builder.view().forms().size() + builder.view().expression_count() + builder.view().pattern_count();
        FormParser parser(input, end, builder, {limits.nodes - used, limits.nesting});
        transaction.commit(parser.parse());
    } catch (const Diagnostic &diagnostic) {
        record(diagnostic);
    }
}

void ParserSession::State::consume(const OrdinaryForm &form) {
    const auto end = form.tokens.empty() ? Token{} : form.tokens.back();
    if (!form.features) {
        record(token_diagnostic(DiagnosticCode::parser_contract, "expanded form has no feature context", end));
        return;
    }
    parse(form.tokens, end, form.features);
}

void ParserSession::State::consume(const Diagnostic &diagnostic) { record(diagnostic); }

void ParserSession::State::consume(const Directive &directive) {
    Diagnostic diagnostic{DiagnosticCode::parser_contract,
                          "preprocessing directive reached the syntax parser",
                          directive.spelling,
                          {},
                          Severity::error,
                          {}};
    record(std::move(diagnostic));
}

ParserSession::ParserSession(ParserLimits limits) : state_(std::make_unique<State>()) { state_->limits = limits; }

ParserSession::~ParserSession() = default;

void ParserSession::parse_form(std::span<const Token> tokens, const Token &end, FeatureSnapshot features) {
    if (!state_) {
        throw std::logic_error("finished parser session");
    }
    state_->parse(tokens, end, std::move(features));
}

void ParserSession::consume(const PreprocessorEvent &event) {
    if (!state_) {
        throw std::logic_error("finished parser session");
    }
    std::visit([this](const auto &value) { state_->consume(value); }, event);
}

bool ParserSession::stopped() const { return !state_ || state_->stopped; }

ParseResult ParserSession::finish(FeatureSnapshot features) && {
    if (!state_) {
        throw std::logic_error("finished parser session");
    }
    auto state = std::move(state_);
    return {std::move(state->builder).finish(std::move(features)), std::move(state->diagnostics), state->failed};
}

ParseResult parse_module(PreprocessorSession &preprocessor, ParserLimits limits) {
    ParserSession parser(limits);
    while (!parser.stopped()) {
        const auto event = preprocessor.next();
        if (!event) {
            break;
        }
        parser.consume(*event);
    }
    auto result = std::move(parser).finish(preprocessor.features());
    result.failed = result.failed || preprocessor.failed();
    return result;
}
} // namespace erlang_aot

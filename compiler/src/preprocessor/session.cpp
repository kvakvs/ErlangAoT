#include "engine.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace erlang_aot {
namespace {
// Read optional includes without conflating an empty file with a missing path.
std::optional<std::string> read_file(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::optional<std::string> environment(std::string_view name) {
    const auto *value = std::getenv(std::string(name).c_str());
    return value ? std::optional<std::string>(value) : std::nullopt;
}

// Preserve token source/location ownership for EOF and initialization errors.
Token start_token(const SourcePtr &source) {
    return {TokenKind::atom, std::u32string{}, {source, 0, 0}, {source->name, 1, 1}, {}};
}
} // namespace

PreprocessorSession::State::State(const SourcePtr &source, PreprocessorOptions settings)
    : options(std::move(settings)) {
    if (options.working_directory.empty()) {
        options.working_directory = std::filesystem::current_path();
    }
    if (!options.read_file) {
        options.read_file = read_file;
    }
    if (!options.environment) {
        options.environment = environment;
    }
    files.push_back({source, Lexer(source), source->name, source->name, {}, {}, 1});
    initialize();
    emit_file(start_token(source), source->name, 1);
}

void PreprocessorSession::State::initialize() {
    macros.reserved = {U"FILE",    U"LINE",          U"FUNCTION_NAME", U"FUNCTION_ARITY",
                       U"MODULE",  U"MODULE_STRING", U"BASE_MODULE",   U"BASE_MODULE_STRING",
                       U"MACHINE", U"BEAM",          U"OTP_RELEASE"};
    macros.undefined = {U"MODULE",        U"MODULE_STRING", U"BASE_MODULE", U"BASE_MODULE_STRING",
                        U"FUNCTION_NAME", U"FUNCTION_ARITY"};
    for (const auto &definition : options.definitions) {
        try {
            predefine(definition);
        } catch (const Diagnostic &error) {
            diagnostic(error);
        } catch (const LexicalError &error) {
            diagnostic(error.diagnostic);
        }
    }
}

void PreprocessorSession::State::predefine(std::string_view definition) {
    const auto equals = definition.find('=');
    const auto name = fragment(std::string(definition.substr(0, equals)));
    if (name.size() != 1 || (name[0].kind != TokenKind::atom && name[0].kind != TokenKind::variable)) {
        pp_fail(DiagnosticCode::macro_arguments, "invalid initial macro name", start_token(files.front().source));
    }
    auto body = fragment(equals == std::string_view::npos ? "true" : std::string(definition.substr(equals + 1)));
    const auto value = parse_term(body, options.limits.expression_depth);
    macros.define({name.front(), std::nullopt, term_tokens(value, name.front())});
}

void PreprocessorSession::State::diagnostic(Diagnostic value) {
    if (value.severity == Severity::error) {
        failed = true;
    }
    for (const auto &file : files) {
        if (!value.location && file.source == value.primary.source) {
            value.location = file.lexer.logical_location(value.primary.begin);
        }
        if (file.include_site) {
            value.related.push_back(file.include_site->spelling);
        }
    }
    pending.emplace_back(std::move(value));
}

bool PreprocessorSession::State::active() const {
    return files.back().branches.empty() || files.back().branches.back().active;
}

std::optional<PreprocessorEvent> PreprocessorSession::State::next() {
    while (pending.empty() && !files.empty()) {
        scan();
    }
    if (pending.empty()) {
        return std::nullopt;
    }
    auto result = std::move(pending.front());
    pending.pop_front();
    return result;
}

void PreprocessorSession::State::scan() {
    keywords();
    try {
        auto tokens = files.back().lexer.form();
        if (tokens.empty()) {
            end_file();
            return;
        }
        files.back().resume_line = tokens.back().location.line;
        if (tokens.back().kind != TokenKind::dot) {
            pp_fail(DiagnosticCode::missing_terminator, "expected form-ending '.'", tokens.back());
        }
        process(std::move(tokens));
    } catch (const LexicalError &error) {
        files.back().lexer.recover_form();
        if (active()) {
            diagnostic(error.diagnostic);
        }
    } catch (const Diagnostic &error) {
        diagnostic(error);
    }
}

void PreprocessorSession::State::process(std::vector<Token> tokens) {
    const auto kind = directive_kind(tokens);
    if (kind && conditional(*kind, tokens)) {
        return;
    }
    if (!active()) {
        return;
    }
    if (!kind) {
        ordinary(expand(tokens, true));
        return;
    }
    auto parsed = parse_directive(tokens);
    if (auto *error = std::get_if<Diagnostic>(&parsed)) {
        throw std::move(*error);
    }
    apply(std::get<Directive>(std::move(parsed)), tokens[1]);
}

void PreprocessorSession::State::apply(Directive directive, const Token &site) {
    switch (directive.kind) {
    case DirectiveKind::define:
        macros.define(std::get<Definition>(std::move(directive.operand)));
        return;
    case DirectiveKind::undef:
        prefix = false;
        macros.undefine(std::get<MacroName>(directive.operand).name.text());
        return;
    case DirectiveKind::include:
    case DirectiveKind::include_lib:
        pp_fail(DiagnosticCode::malformed_directive, "include effects await step 8", site);
        return;
    default:
        break;
    }
    pp_fail(DiagnosticCode::malformed_directive, "directive effect is not implemented at this step", site);
}

void PreprocessorSession::State::end_file() {
    if (!files.back().branches.empty()) {
        const auto branch = files.back().branches.back();
        files.back().branches.pop_back();
        pp_fail(DiagnosticCode::conditional_structure, "unterminated conditional", branch.opening);
    }
    files.pop_back();
    if (!files.empty()) {
        const auto &file = files.back();
        emit_file(start_token(file.source), file.logical_name, file.resume_line);
    }
}

PreprocessorSession::PreprocessorSession(const SourcePtr &source, PreprocessorOptions options)
    : state_(std::make_unique<State>(source, std::move(options))) {}

PreprocessorSession::~PreprocessorSession() = default;

bool PreprocessorSession::failed() const { return state_->failed; }

std::optional<PreprocessorEvent> PreprocessorSession::next() { return state_->next(); }
} // namespace erlang_aot

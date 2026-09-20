#include "engine.hpp"

namespace erlang_aot {
std::string PreprocessorSession::State::expand_environment(std::string name) const {
    if (name.empty() || name.front() != '$') {
        return name;
    }
    const auto slash = name.find_first_of("/\\");
    const auto key = std::string_view(name).substr(1, slash == std::string::npos ? slash : slash - 1);
    const auto value = options.environment(key);
    if (value) {
        name.replace(0, slash == std::string::npos ? name.size() : slash, *value);
    }
    return name;
}

namespace {
// Application lookup is a deterministic fallback after ordinary include paths.
void append_application(std::vector<std::filesystem::path> &result, const std::filesystem::path &path,
                        const PreprocessorOptions &options) {
    if (path.begin() != path.end()) {
        auto component = path.begin();
        const auto app = options.applications.find(component->string());
        if (app != options.applications.end()) {
            auto candidate = app->second;
            if (candidate.is_relative()) {
                candidate = options.working_directory / candidate;
            }
            for (++component; component != path.end(); ++component) {
                candidate /= *component;
            }
            result.push_back(std::move(candidate));
        }
    }
}
} // namespace

std::vector<std::filesystem::path> PreprocessorSession::State::candidates(const std::string &name, bool library) const {
    const std::filesystem::path path(name);
    if (path.is_absolute()) {
        return {path};
    }
    auto directory = files.back().path.parent_path();
    if (directory.is_relative()) {
        directory = options.working_directory / directory;
    }
    std::vector<std::filesystem::path> result{directory / path, options.working_directory / path};
    for (auto include : options.include_paths) {
        if (include.is_relative()) {
            include = options.working_directory / include;
        }
        result.push_back(include / path);
    }
    if (library) {
        append_application(result, path, options);
    }
    return result;
}

void PreprocessorSession::State::push_file(const SourcePtr &source, std::filesystem::path path, const Token &site) {
    files.push_back({source, Lexer(source), std::move(path), source->name, {}, site, 1});
    if (options.include_loaded) {
        options.include_loaded(files.back().path);
    }
    emit_file(site, source->name, 1);
}

void PreprocessorSession::State::include(const Directive &directive, const Token &site) {
    if (files.size() >= options.limits.include_depth) {
        pp_fail(DiagnosticCode::resource_limit, "include depth exhausted", site);
    }
    const auto &tokens = std::get<TokenOperand>(directive.operand).tokens;
    std::u32string name;
    for (const auto &token : tokens) {
        if (token.kind != TokenKind::string) {
            pp_fail(DiagnosticCode::malformed_directive, "include expects literal strings", token);
        }
        name += token.text();
    }
    const auto filename = expand_environment(utf8(name));
    for (const auto &path : candidates(filename, directive.kind == DirectiveKind::include_lib)) {
        const auto bytes = options.read_file(path);
        if (!bytes) {
            continue;
        }
        try {
            push_file(sources.add(path.string(), *bytes), path, site);
        } catch (const EncodingError &error) {
            pp_fail(DiagnosticCode::invalid_encoding, path.string() + ": " + error.what(), site);
        }
        return;
    }
    pp_fail(DiagnosticCode::include_not_found, "cannot find include " + filename, site);
}
} // namespace erlang_aot

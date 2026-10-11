// Added for parse transforms: .abstr files as compiler inputs.
#include "abstract_input.hpp"
#include "import.hpp"
#include "term_text.hpp"

namespace clause::transforms {
SourcePtr existing_source(SourceManager &sources, const std::string &file) {
    const std::filesystem::path path(std::u8string(file.begin(), file.end()));
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
        return nullptr;
    }
    try {
        return sources.read(path);
    } catch (const std::exception &) {
        return nullptr;
    }
}

AbstractFile parse_abstract_file(const std::filesystem::path &path, const PreprocessorOptions &options,
                                 const ParserSession &parser) {
    SourceManager sources;
    const auto text = sources.read(path);
    Terms terms;
    const auto forms = read_text(text, terms);
    const auto bytes = path.generic_u8string();
    const std::string name(bytes.begin(), bytes.end());
    // An empty preprocessing session yields the module's initial features from the options.
    const PreprocessorSession defaults(sources.add(name, ""), options);
    const ImportOptions import{
        .main_file_ = name,
        .source_ = [&sources](const std::string &file) { return existing_source(sources, file); },
        .features_ = defaults.features()};
    auto result = import_forms(terms, forms, import, parser);
    return {.diagnostics_ = std::move(result.diagnostics_),
            .features_ = import.features_,
            .end_ = result.eof_.value_or(Position{.byte = 0, .line = 1, .column = 1})};
}
} // namespace clause::transforms

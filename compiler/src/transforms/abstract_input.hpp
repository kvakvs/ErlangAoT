#pragma once
// Added for parse transforms: .abstr files as compiler inputs, like erlc File.abstr.
#include <clause/compiler/parser.hpp>
#include <filesystem>

namespace clause::transforms {
struct AbstractFile {
    // Problems of the file's forms, the module's features and where its eof form says the source ended.
    std::vector<Diagnostic> diagnostics_;
    FeatureSnapshot features_;
    Position end_{};
};

// A file named by imported forms, read when it exists so imported code keeps its debug lines; else null.
SourcePtr existing_source(SourceManager &sources, const std::string &file);

// Read a file of abstract format forms (file:consult syntax) and parse them into `parser` as one module; the
// files named by its file attributes supply debug lines when they exist. Unreadable text throws TermError.
AbstractFile parse_abstract_file(const std::filesystem::path &path, const PreprocessorOptions &options,
                                 const ParserSession &parser);
} // namespace clause::transforms

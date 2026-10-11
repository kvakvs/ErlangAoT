#pragma once
// Added for parse transforms: abstract format forms back into Clause syntax (docs/transforms.md).
#include "terms.hpp"
#include <clause/compiler/parser.hpp>
#include <functional>
#include <span>

namespace clause::transforms {
struct ImportOptions {
    // The file of forms before the first file attribute.
    std::string main_file_;
    // The source behind a file name, so imported code keeps debug lines and source comments of its real file;
    // null when the file is unknown.
    std::function<SourcePtr(const std::string &)> source_;
    // Features of the module whose forms these are.
    FeatureSnapshot features_;
};

struct ImportResult {
    // Problems found while importing, and the location of the {eof, Location} form when there is one.
    std::vector<Diagnostic> diagnostics_;
    std::optional<Position> eof_;
};

// Parse each form through the parser, as tokens located at the form's annotations; file attributes switch the
// file of the following forms. Malformed forms, {error, _} and {warning, _} forms become diagnostics (error forms
// and malformed ones are errors) and are skipped; parsing stops at {eof, _}.
ImportResult import_forms(const Terms &terms, std::span<const TermId> forms, const ImportOptions &options,
                          const ParserSession &parser);
} // namespace clause::transforms

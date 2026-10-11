#pragma once
// Added for parse transforms: a parsed module as OTP abstract format forms (docs/transforms.md).
#include "terms.hpp"
#include <clause/compiler/ast/module.hpp>
#include <span>

namespace clause::transforms {
struct AbstractModule {
    // The arena holding every form and the forms in order, ending with {eof, Location}.
    Terms terms_;
    std::vector<TermId> forms_;
};

// Export `forms` of the module as erl_parse/epp would produce them, with {Line, Column} annotations; the eof form
// is the end of the main source file, counted from its last -file directive. Unsupported values throw TermError.
AbstractModule export_module(const ast::Module &syntax, std::span<const ast::FormId> forms, const Source &main);
// Append each form's consult text on its own line.
std::string abstract_text(const AbstractModule &module);
} // namespace clause::transforms

#pragma once
// Added for parse transforms: the file:consult/1 text of .abstr files and --print-abstr.
#include "terms.hpp"
#include <clause/compiler/source.hpp>
#include <string>
#include <vector>

namespace clause::transforms {
// Append the canonical text of one term: no layout spaces, atoms quoted only when needed, printable lists as
// strings, floats in their shortest exact form.
void write_text(std::string &out, const Terms &terms, TermId id);
// Read every `Term.` of a consult file in order; malformed text throws TermError naming file, line and column.
std::vector<TermId> read_text(const SourcePtr &source, Terms &terms);
} // namespace clause::transforms

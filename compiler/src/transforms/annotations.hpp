#pragma once
// Added for parse transforms: erl_parse's first_anno/last_anno over abstract terms.
#include "terms.hpp"

namespace clause::transforms {
// Whether a term is a {Line, Column} annotation.
bool annotation(const Terms &terms, TermId id);
// The first annotation of a node: its own, replaced by each following one in prefix order while they do not move
// forward (erl_parse:first_anno); none when the node has none.
std::optional<TermId> first_annotation(const Terms &terms, TermId node);
// The latest annotation anywhere inside a node (erl_parse:last_anno); none when the node has none.
std::optional<TermId> last_annotation(const Terms &terms, TermId node);
// Whether two {Line, Column} annotations name the same place.
bool same_place(const Terms &terms, TermId left, TermId right);
} // namespace clause::transforms

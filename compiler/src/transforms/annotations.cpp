// Added for parse transforms: erl_parse's first_anno/last_anno over abstract terms, without recursion.
#include "annotations.hpp"
#include <span>

namespace clause::transforms {
namespace {
// An annotation's location as a comparable pair.
std::pair<std::int64_t, std::int64_t> position(const Terms &terms, const TermId anno) {
    const auto &node = terms.node(anno);
    return {*terms.small_integer(node.children_[0]), *terms.small_integer(node.children_[1])};
}

// The children of an abstract node that may hold annotations: all of them, except a bin_element's type list.
std::span<const TermId> annotated_children(const Terms &terms, const TermNode &node) {
    std::span<const TermId> children = node.children_;
    if (node.kind_ == TermKind::tuple && !children.empty() && terms.is_atom(children[0], U"bin_element")) {
        return children.first(std::min<std::size_t>(children.size(), 4));
    }
    return children;
}

// Queue a node's children so the first one is visited next.
void queue(const Terms &terms, const TermId id, std::vector<TermId> &pending) {
    const auto children = annotated_children(terms, terms.node(id));
    for (auto child = children.rbegin(); child != children.rend(); ++child) {
        pending.push_back(*child);
    }
}
} // namespace

bool annotation(const Terms &terms, const TermId id) {
    const auto &node = terms.node(id);
    return node.kind_ == TermKind::tuple && node.children_.size() == 2 && terms.small_integer(node.children_[0]) &&
           terms.small_integer(node.children_[1]);
}

std::optional<TermId> first_annotation(const Terms &terms, const TermId node) {
    std::optional<TermId> result;
    std::vector<TermId> pending{node};
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (!annotation(terms, id)) {
            queue(terms, id, pending);
        } else if (result && position(terms, *result) < position(terms, id)) {
            return result;
        } else {
            result = id;
        }
    }
    return result;
}

std::optional<TermId> last_annotation(const Terms &terms, const TermId node) {
    std::optional<TermId> result;
    std::vector<TermId> pending{node};
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (!annotation(terms, id)) {
            queue(terms, id, pending);
        } else if (!result || position(terms, *result) < position(terms, id)) {
            result = id;
        }
    }
    return result;
}

bool same_place(const Terms &terms, const TermId left, const TermId right) {
    return annotation(terms, left) && annotation(terms, right) && position(terms, left) == position(terms, right);
}
} // namespace clause::transforms

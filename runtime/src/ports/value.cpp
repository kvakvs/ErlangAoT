#include "value.hpp"
#include <clause/runtime/process_context.hpp>

namespace clause::runtime::detail {
namespace {
// Bytes as a binary or a list of small integers.
TermResult<Term> bytes_term(TermFactory &factory, const PortValue &value) {
    if (value.binary) {
        return factory.binary(value.bytes);
    }
    std::vector<Word> words;
    words.reserve(value.bytes.size());
    for (const auto byte : value.bytes) {
        words.push_back(encode_integer(std::to_integer<std::int64_t>(byte)).value_or(0));
    }
    const auto nil = factory.nil();
    return nil ? factory.list_words(words, *nil) : nil;
}
} // namespace

TermResult<Term> build_value(ProcessContext &process, const PortValue &value) {
    TermFactory factory(process);
    switch (value.kind) {
    case PortValue::Kind::atom:
        return factory.atom(value.atom);
    case PortValue::Kind::integer:
        return factory.integer(value.integer);
    case PortValue::Kind::bytes:
        return bytes_term(factory, value);
    case PortValue::Kind::identity:
        return Term::from_word(value.identity, process);
    case PortValue::Kind::tuple:
        break;
    }
    std::vector<Term> elements;
    for (const auto &element : value.elements) {
        const auto term = build_value(process, element);
        if (!term) {
            return term;
        }
        elements.push_back(*term);
    }
    return factory.tuple(elements);
}
} // namespace clause::runtime::detail

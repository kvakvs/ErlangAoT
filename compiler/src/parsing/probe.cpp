#include <erlang_aot/compiler/probe.hpp>
#include "boost_parser.hpp"

namespace erlang_aot {
namespace {
namespace bp = boost::parser;

const auto letter = bp::char_('a', 'z') | bp::char_('A', 'Z') | bp::char_('_');
const auto identifier = bp::lexeme[letter >> *(letter | bp::char_('0', '9') | bp::char_('@'))];
constexpr bp::rule<class nested> nested = "nested arguments";
const auto nested_def = bp::lit('(') >> *(nested | bp::omit[bp::char_ - bp::char_("()")]) >> ')';
BOOST_PARSER_DEFINE_RULES(nested)

// Public prefix_parse is constrained to character iterators, not Token iterators.
struct TokenProbe {
    // Represent a token category to probe the iterator constraint.
    int kind;
};
static_assert(!bp::parsable_iter<TokenProbe*>);
} // namespace

std::optional<DirectiveProbe> probe_directive(std::string_view source)
{
    const auto grammar = '-' >> bp::raw[identifier] >> nested >> '.' >> bp::eoi;
    const auto parsed = bp::parse(source, grammar, bp::ws);
    if (!parsed) {
        return std::nullopt;
    }
    const auto begin = static_cast<std::size_t>(parsed->begin() - source.begin());
    return DirectiveProbe{std::string(parsed->begin(), parsed->end()), begin,
        begin + static_cast<std::size_t>(parsed->size())};
}
} // namespace erlang_aot

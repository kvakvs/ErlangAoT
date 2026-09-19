#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <random>
#include <sstream>
#include <stdexcept>

using namespace erlang_aot;

// Produce valid owned token values, then mutate only token order/presence.
std::vector<Token> scan(std::string text) {
    SourceManager sources;
    Lexer lexer(sources.add("mutations.erl", std::move(text)));
    std::vector<Token> result;
    while (auto token = lexer.next())
        result.push_back(std::move(*token));
    return result;
}

// One malformed form must terminate and leave the following valid form reachable.
std::string run(const std::vector<Token> &input) {
    ParserLimits limits;
    limits.nodes = 10000;
    limits.work = 40000;
    limits.nesting = 64;
    ParserSession parser(limits);
    parser.parse_form(input, input.back());
    const auto good = scan("recovered() -> ok.");
    parser.parse_form(good, good.back());
    auto result = std::move(parser).finish();
    if (result.module.forms().empty())
        throw std::runtime_error("mutation lost subsequent form");
    const auto &last = std::get<ast::Function>(result.module.form(result.module.forms().back()).value);
    if (last.name.name != U"recovered")
        throw std::runtime_error("mutation recovery mismatch");
    std::ostringstream record;
    for (const auto &diagnostic : result.diagnostics)
        record << static_cast<unsigned>(diagnostic.code) << ':' << diagnostic.primary.begin << ':' << diagnostic.message
               << '\n';
    print_ast(record, result.module, 40000);
    return record.str();
}

int main() {
    std::mt19937 random(0x29a016);
    for (const auto &source :
         {"f(A) -> case A of {X,Y} -> [X,Y]; _ -> <<1:8>> end.",
          "-type t(A) :: #{atom() => [A,...]} | fun((A) -> {ok,A}).", "-spec f(A) -> A when is_subtype(A, integer()).",
          "f(L) -> [X || X <- L, X > 0].", "-custom({f/1,#{key => [1,2,3]}})."}) {
        const auto original = scan(source);
        for (int iteration = 0; iteration < 180; ++iteration) {
            auto tokens = original;
            const auto index = static_cast<std::size_t>(random()) % (tokens.size() - 1);
            if (iteration % 3 == 0)
                tokens.erase(tokens.begin() + static_cast<std::ptrdiff_t>(index));
            else if (iteration % 3 == 1)
                tokens.insert(tokens.begin() + static_cast<std::ptrdiff_t>(index), original[index]);
            else
                std::swap(tokens[index], tokens[(index + 1) % (tokens.size() - 1)]);
            if (run(tokens) != run(tokens))
                throw std::runtime_error("nondeterministic mutation result");
        }
    }
}

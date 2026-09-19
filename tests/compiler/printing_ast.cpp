#include <erlang_aot/compiler/parser.hpp>
#include <erlang_aot/compiler/printing.hpp>
#include <sstream>
#include <stdexcept>

namespace {
// Keep assertions active in every build configuration and name the failed contract.
void require(bool condition, const char *message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

// Return an AST whose source manager and preprocessing session have already been destroyed.
erlang_aot::ast::Module parse(std::string text) {
    erlang_aot::SourceManager sources;
    erlang_aot::PreprocessorSession session(sources.add("tree.erl", std::move(text)));
    auto result = erlang_aot::parse_module(session);
    require(result.succeeded(), "tree fixture must parse");
    return std::move(result.module);
}

// Verify exact roles, ordering, compact scalar fields, and whitespace on a small tree.
void layout() {
    const auto module = parse("-module(tree). f(X) -> {X, 42}.");
    std::ostringstream output;
    erlang_aot::print_ast(output, module);
    require(output.str() == "Module forms=3\n"
                            "  form[0]: FileAttribute name=\"tree.erl\" line=1\n"
                            "  form[1]: ModuleAttribute name=tree\n"
                            "  form[2]: Function name=f arity=1 clauses=1\n"
                            "    clause[0]: FunctionClause arguments=1 guard=none body=1\n"
                            "      argument[0]: RestrictedPattern\n"
                            "        expression: Variable name=X\n"
                            "      body[0]: Tuple elements=2\n"
                            "        element[0]: Variable name=X\n"
                            "        element[1]: IntegerLiteral value=42\n",
            "indented tree layout changed");
}

// Exercise typed distinctions and ensure literal newlines cannot split object lines.
void values() {
    const auto module = parse(R"erl(-module(tree).
f(X) when is_integer(X), X > 0; X =:= 0 ->
  {[], [X|tail], (X), 1.25, $\n, "a\n\"λ", 'fun',
   -X, X + 1, X = 1, catch m:f(X),
   #{a => 1}, X#{a := 2}, #r{a=1,_=X}, X#r.a, #r.a,
   #m:r{a=1}, X#_{a=1}, <<>>, <<X:8/integer-unit:8, X>>, ~b"λ"};
f(_) -> ok.
)erl");
    std::ostringstream output;
    erlang_aot::print_ast(output, module);
    const auto text = output.str();
    for (const auto *expected : {"clauses=2",
                                 "GuardSyntax alternatives=2",
                                 "GuardConjunction tests=2",
                                 "[count=0 tail=nil",
                                 "[count=1 tail=explicit",
                                 "Group",
                                 "FloatLiteral value=1.25",
                                 "CharacterLiteral value=$\\n",
                                 "StringLiteral value=\"a\\n\\\"λ\"",
                                 "Atom name='fun'",
                                 "UnaryExpression operator=-",
                                 "BinaryExpression operator=+",
                                 "MatchExpression",
                                 "CatchExpression",
                                 "CallExpression arguments=1",
                                 "RemoteExpression",
                                 "MapExpression mode=construct",
                                 "MapExpression mode=update",
                                 "MapField operator=:=",
                                 "RecordExpression mode=construct identity=local name=r",
                                 "RecordField Variable name=_",
                                 "RecordAccess identity=local name=r field=a",
                                 "RecordIndex record=r field=a",
                                 "identity=qualified module=m name=r",
                                 "identity=inferred",
                                 "Bitstring segments=0",
                                 "BinarySegment size=explicit modifiers=2",
                                 "BinarySegment size=omitted modifiers=omitted",
                                 "BinaryModifier name=unit parameter=8",
                                 "BinaryModifier name=utf8 parameter=none"}) {
        require(text.find(expected) != std::string::npos, expected);
    }
}

// Long left-associated chains must print without recursion or quadratic indentation output.
void deep_tree() {
    std::string source = "f() -> 0";
    for (int index = 0; index < 9000; ++index) {
        source += "+1";
    }
    auto module = parse(source + '.');
    std::ostringstream output;
    erlang_aot::print_ast(output, module);
    const auto text = output.str();
    require(text.find("[depth=9000]") != std::string::npos, "deep tree depth must remain visible");
    require(text.size() < 4000000, "deep tree indentation must stay bounded");
    require(text.ends_with("right: IntegerLiteral value=1\n"), "deep tree must finish printing");
}
} // namespace

// Exercise public printing independently of CLI option routing.
int main() {
    layout();
    values();
    deep_tree();
    std::ostringstream output;
    erlang_aot::print_ast(output, erlang_aot::ast::Module{});
    require(output.str() == "Module forms=0\n", "empty module must have a root");
}

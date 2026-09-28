#include <erlang_aot/compiler/preprocessor.hpp>
#include <iostream>
#include <stdexcept>
using namespace erlang_aot;

// Raw reader events are an embedding API; the production CLI expands macros instead.
void require(bool condition) {
    if (!condition) {
        throw std::runtime_error("raw directive reader contract failed");
    }
}

// Sessions retain independent failure state and leave expansion to their consumer.
void reader_contract() {
    SourceManager sources;
    DirectiveReader first(sources.add("first", "-undef(). good()."));
    DirectiveReader second(sources.add("second", "-define(X, ok). ?X."));
    require(std::holds_alternative<Diagnostic>(*first.next()));
    require(std::holds_alternative<Directive>(*second.next()));
    require(first.failed() && !second.failed());
    require(std::holds_alternative<OrdinaryForm>(*first.next()));
    require(std::get<OrdinaryForm>(*second.next()).tokens.front().text() == U"?");
    require(!first.next() && !first.next() && first.failed());
    require(!render(std::get<Diagnostic>(parse_directive({}))).empty());
}

// Raw-reader misplaced-directive policy intentionally differs from the expanding session.
void misplaced_contract() {
    SourceManager sources;
    DirectiveReader reader(sources.add("raw.erl", "f() ->\n  -define(X, 1).\nnext() -> ok."));
    const auto error = std::get<Diagnostic>(*reader.next());
    require(error.code == DiagnosticCode::misplaced_directive);
    require(error.primary.source->position(error.primary.begin).line == 2);
    require(std::holds_alternative<OrdinaryForm>(*reader.next()));
}

// Normal directives, malformed envelopes and bounded recovery now use CLI fixtures.
int main() {
    try {
        reader_contract();
        misplaced_contract();
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}

-module(sigils).
-define(RAW, ~S{a\nb"c}).
-define(ID(X), X).
f() -> {?RAW, ?ID(~b"a\n\""), ~s/a\tb/, ~B"Î»", ~"default\n", ~custom|raw\n|suffix}.
g() -> ~S"""
    raw\text
    """.
h() -> ~b"""
    escaped\ntext
    """.

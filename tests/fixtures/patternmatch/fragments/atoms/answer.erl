-module(answer).
-export([id/1, truth/0, falsity/0, ok_value/0, unicode/0, empty/0, nul/0, projected/0]).
-spec truth() -> integer().
truth() -> true.
falsity() -> false.
ok_value() -> ok.
unicode() -> 'λ😀'.
empty() -> ''.
nul() -> 'a\x{0}b'.
id(Value) ->
    Saved = Value,
    Saved.
projected() -> id((true)).

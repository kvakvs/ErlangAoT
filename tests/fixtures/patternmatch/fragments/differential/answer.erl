-module(answer).
-export([value/0, identity/1, first/2, second/2, negative/0]).
value() -> 42.
identity(X) -> X.
first(Selected, Discarded) ->
    _ = Discarded,
    Saved = Selected,
    Saved.
second(_, Y) -> Y.
negative() -> -17.

-spec value() -> atom() | integer().
-spec identity(integer()) -> integer().

-module(answer).
-export([value/0, identity/1]).
-spec value() -> integer().
value() -> 42.
identity(X) -> X.

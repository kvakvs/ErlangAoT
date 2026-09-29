-module(answer).
-export([value/0, identity/1, first/2, second/2, negative/0]).
value() -> 42.
identity(X) -> X.
first(X, _) -> X.
second(_, Y) -> Y.
negative() -> -17.

-module(parameters).
-export([identity/1, first/3, second/3, third/3]).
-spec identity(integer()) -> integer().
identity(X) -> (X).
first(X, _, _) -> X.
second(_, X, _) -> X.
third(_, _, X) -> X.

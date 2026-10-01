-module(failure_answer).
-export([chain/0, later/0, take/2]).
chain() -> leaf().
leaf() -> 42.
later() -> 7.
take(X, _) -> X.

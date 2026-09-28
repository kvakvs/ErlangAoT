-module(guards).
-export([select/2, budgeted/2]).
-spec select(integer(), integer()) -> integer().
select(X, _) -> X.
budgeted(X, _) -> X.

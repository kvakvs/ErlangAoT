%% Narrower inferred types, unknown facts and the constructs the check admits compile without an error.
-module(accepted).
-export([
    narrower/0,
    unknown/1,
    dynamic/0,
    raises/0,
    unconstrained/1,
    external/0,
    callback/0,
    other_opaque/0,
    waits/0
]).
-callback callback() -> atom().
-spec narrower() -> integer().
narrower() -> 1.
-spec unknown(tuple()) -> atom().
unknown(X) -> element(1, X).
-spec dynamic() -> dynamic().
dynamic() -> 1.
%% A function that never returns fits any result.
-spec raises() -> integer().
raises() -> error(stop).
-spec unconstrained(A) -> A.
unconstrained(X) -> X.
%% An unresolved remote type admits everything.
-spec external() -> unknown_module:t().
external() -> 1.
%% Callback specifications are not checked against implementations.
callback() -> 1.
%% Outside its module an opaque type stays closed.
-spec other_opaque() -> opaque:secret().
other_opaque() -> ok.
%% A receive that waits for ever returns only through its clauses: here never.
-spec waits() -> no_return().
waits() ->
    receive
    after infinity -> ok
    end.

-module(thrower).
-export([depth/1, rethrow/1, guarded/1]).
-compile(nowarn_deprecated_catch).

%% Remote helpers raise below a fixed call depth or turn a caught exception into a new one.
depth(N) -> {depth, one(N)}.

one(N) -> [one | two(N)].

two(0) -> [returned];
two(N) -> throw({deep, N}).

rethrow(Reason) ->
    {'EXIT', {Caught, _}} = (catch error(Reason)),
    throw({rethrown, Caught}).

%% A caught failure inside a case clause body lets the next clause body continue normally.
guarded(Value) ->
    Checked = (catch check(Value)),
    case Checked of
        {'EXIT', {function_clause, _}} -> {rejected, Value};
        Result -> {accepted, Result}
    end.

check(fine) -> ok.

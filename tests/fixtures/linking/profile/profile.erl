-module(profile).
-export([main/1]).

% A program with one hot function: the profiling test checks that it ranks first by self time and by entries, and
% that the busy process ranks first among processes (docs/profiling.md).

% Hot: two million entries of a small arithmetic loop.
spin(0, Acc) -> Acc;
spin(N, Acc) -> spin(N - 1, (Acc + N) rem 1000003).

% Cold: a few entries.
cold(X) -> X + 1.

main(_) ->
    Self = self(),
    spawn(fun() -> Self ! {spun, spin(2000000, 0)} end),
    Spun =
        receive
            {spun, Value} -> Value
        end,
    io:format("~p~n", [{cold(Spun), Spun}]).

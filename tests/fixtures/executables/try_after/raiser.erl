-module(raiser).
-export([depth/1, wrapped/1]).

%% Remote helpers raise below a fixed call depth or clean up with their own after body.
depth(N) -> {depth, one(N)}.

one(N) -> [one | two(N)].

two(0) -> [returned];
two(N) -> throw({deep, N}).

wrapped(Mode) ->
    try
        work(Mode)
    after
        erlang:display({cleanup, Mode})
    end.

work(ok) -> done;
work(fail) -> error(failed).

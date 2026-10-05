-module(deep_recursion).
-export([main/1]).

%% Deeper than an 8 MiB native stack holds at 42 bytes per native frame; the
%% abandoned native-call lowering needed about 80.
-define(DEPTH, 200000).

%% The first argument selects a body-recursive scenario of ?DEPTH calls.
main(["build"]) ->
    List = build(id(?DEPTH)),
    [First | _] = List,
    show(First),
    show(len(List)),
    show(sum(List));
main(["mutual"]) ->
    show(down(id(?DEPTH)));
main(["nested"]) ->
    show(depth(nest(id(?DEPTH), leaf)));
main(["unwind"]) ->
    show(
        try
            boom(id(?DEPTH))
        catch
            error:Reason -> {caught, Reason}
        end
    );
main(["budget"]) ->
    show(forever(id(0))).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

%% A list built and consumed by body recursion.
build(0) -> [];
build(N) -> [N | build(N - 1)].

len([]) -> 0;
len([_ | Tail]) -> 1 + len(Tail).

sum([]) -> 0;
sum([Head | Tail]) -> Head + sum(Tail).

%% Mutual body recursion.
down(0) -> 0;
down(N) -> 1 + up(N - 1).

up(0) -> 0;
up(N) -> 1 + down(N - 1).

%% A deeply nested term built by a tail loop and measured by body recursion.
nest(0, Acc) -> Acc;
nest(N, Acc) -> nest(N - 1, {node, Acc}).

depth(leaf) -> 0;
depth({node, Inner}) -> 1 + depth(Inner).

%% An exception raised at the bottom unwinds every frame to the handler.
boom(0) -> error(bottom);
boom(N) -> 1 + boom(N - 1).

%% Body recursion that never ends: ErlangAoT stops it at the stack budget.
forever(N) -> 1 + forever(N + 1).

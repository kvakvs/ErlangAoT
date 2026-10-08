-module(identity_guards).
-export([main/1, id/1]).

-record(#point{x = 0, y = 0}).

id(X) -> X.

% Guards over pids, references, funs and native records with real values of every kind.
kind(X) when is_pid(X), X =:= self() -> self;
kind(X) when is_pid(X), node(X) =:= node() -> pid;
kind(X) when is_reference(X), node(X) =:= nonode@nohost -> reference;
kind(X) when is_function(X, 2) -> fun2;
kind(X) when is_function(X) -> function;
kind(X) when is_record(X) -> native_record;
kind(_) -> other.

% node/1 of anything else fails the guard; in a body it raises badarg.
named(X) when node(X) =:= nonode@nohost -> named;
named(_) -> unnamed.

% A receive guard comparing with self().
mine() ->
    receive
        {From, Message} when From =:= self() -> {mine, Message}
    after 0 -> none
    end.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

main(_) ->
    Other = spawn(fun() -> ok end),
    Values = [
        self(),
        Other,
        make_ref(),
        fun id/1,
        fun(A, B) -> {A, B} end,
        fun erlang:node/0,
        #point{x = 1},
        {point, 1, 2},
        atom,
        1
    ],
    io:format("~p~n", [[kind(V) || V <- Values]]),
    io:format("~p~n", [[named(V) || V <- Values]]),
    self() ! {self(), hello},
    io:format("~p~n", [mine()]),
    io:format("~p~n", [
        {node(), node(self()), node(make_ref()), is_record(#point{}), is_record(?MODULE:id(1))}
    ]),
    Node = fun erlang:node/0,
    io:format("~p~n", [{Node(), apply(erlang, node, [self()]), (fun erlang:is_record/1)(#point{})}]),
    io:format("~p~n", [[error_of(fun() -> node(?MODULE:id(V)) end) || V <- [1, atom, "pid"]]]).

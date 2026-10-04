-module(depth).
-export([one/2, pick/1]).

%% A remote call chain whose innermost function raises the requested class with a nested reason.
one(Class, Reason) ->
    erlang:display({one, Class}),
    two(Class, {one, Reason}).

two(Class, Reason) ->
    Value = three(Class, [two | Reason]),
    erlang:display({two_returned, Value}),
    Value.

three(error, Reason) -> erlang:error(Reason);
three(exit, Reason) -> exit(Reason);
three(throw, Reason) -> erlang:throw(Reason).

pick(1) -> one;
pick(2) -> two.

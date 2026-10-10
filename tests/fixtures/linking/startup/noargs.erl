%% An entry without arguments runs without the program arguments; one with the argument list is preferred.
-module(noargs).
-export([start/0, both/0, both/1]).

start() -> erlang:display(started).

both() -> erlang:display(zero).

both(Arguments) -> erlang:display({one, Arguments}).

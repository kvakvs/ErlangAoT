-module(client).
-export([value/0, demo/0, main/1]).
value() -> answer:identity(answer:value()).
demo() -> answer:demo().
%% Executable entry (clau -o): print the values the native harness prints.
main(_Args) ->
    erlang:display(value()),
    erlang:display(answer:identity(-7)),
    erlang:display(demo()).

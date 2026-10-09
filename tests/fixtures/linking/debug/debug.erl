-module(debug).
-export([main/1]).

-include("debug.hrl").

% Lines of these functions are what the debugger tests break on (docs/debugging.md).
leaf(N) ->
    erlang:display({leaf, N}),
    N + 1.

middle(N) ->
    R = leaf(twice(N)),
    {middle, R}.

main(_) ->
    erlang:display(middle(20)).

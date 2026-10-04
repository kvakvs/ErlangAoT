-module(shadow).
-compile({no_auto_import, [error/1]}).
-export([error/1, fail/1]).

%% With error/1 auto-import suppressed, the unqualified call reaches this local function.
error(Reason) -> {not_raised, Reason}.

fail(Reason) ->
    erlang:display(error(Reason)),
    erlang:error(Reason).

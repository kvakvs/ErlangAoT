-module(undefined).
-export([main/1]).
-compile({parse_transform, no_such_transform}).

main(_) ->
    ok.

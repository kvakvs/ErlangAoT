-module(refused).
-export([main/1]).
-compile([debug_info, {parse_transform, pt_error}]).

main(_) ->
    ok.

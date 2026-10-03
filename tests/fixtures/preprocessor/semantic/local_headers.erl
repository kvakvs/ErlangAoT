-module(local_headers).
-include("headers/checks.hrl").
-include("headers/entry.hrl").
check(Value) ->
    ?CHECK(is_integer(Value)),
    ?EQUAL(1, Value),
    ?MATCH({local, _}, {local, Value}),
    ?RAISES(error, local_failure, erlang:error(local_failure)),
    #entry{bytes = 0, payload = Value}.

-module(client).
-export([run/1, retry/1]).
-spec run(integer()) -> integer() | tuple().
run(X) -> answer:allocation_spec(answer:identity_spec(X)).
retry(X) -> answer:joined_plain(X).

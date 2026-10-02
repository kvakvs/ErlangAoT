-module(client).
-export([run/1,retry/1]).
run(X) -> answer:staged(answer:identity(X)).
retry(X) -> answer:guard(X).

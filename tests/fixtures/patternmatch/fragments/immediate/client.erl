-module(client).
-export([nested/1, retry/1]).
nested(X) -> answer:repeat(answer:literal(X), no).
retry(X) -> answer:aliases(X).

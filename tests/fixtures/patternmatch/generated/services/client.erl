-module(client).
-export([nested/1,retry/1]).
nested(X) -> answer:body_is_atom(answer:head_value(X)).
retry(X) -> answer:qualified(X).

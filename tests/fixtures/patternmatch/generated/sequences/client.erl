-module(client).
-export([nested/1,stop/1,retry/1]).
nested(X) -> answer:stop(answer:bind(X)), answer:id(unreachable).
stop(X) -> answer:body_failure(X).
retry(X) -> Y = answer:bind(X), Y.

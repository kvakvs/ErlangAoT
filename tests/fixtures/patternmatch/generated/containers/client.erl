-module(client).
-export([nested/1,retain/1,fail/1]).
nested(X) -> answer:id(answer:first(X, answer:make(X))).
retain(X) -> A = answer:extract(X), answer:make(A), answer:id(A).
fail(X) -> answer:badmatch(X).

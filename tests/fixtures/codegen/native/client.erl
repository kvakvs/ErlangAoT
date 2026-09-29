-module(client).
-export([value/0, nested/2]).
value() -> answer:identity(answer:value()).
nested(X, Y) -> answer:first(answer:second(Y, X), answer:identity(Y)).

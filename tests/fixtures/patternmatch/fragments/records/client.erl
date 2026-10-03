-module(client).
-export([retain/1, nested/1]).
retain(X) ->
    R = answer:retained(X),
    answer:default_calls(),
    answer:id(R).
nested(X) -> answer:access(answer:id(X)).

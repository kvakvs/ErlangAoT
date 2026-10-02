-module(client).
-export([nested/2,retain/1]).
nested(X,Y) -> answer:my_add(answer:my_div(X,Y),X).
retain(X) -> A = answer:roundtrip(X), answer:independent(X), answer:id(A).

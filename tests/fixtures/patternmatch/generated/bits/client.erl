-module(client).
-export([retain/1,nested/2]).
retain(B) -> {T,R} = answer:shared(B), answer:literal(), {answer:id(T),answer:id(R)}.
nested(X,N) -> answer:read(N,answer:little(X,N)).

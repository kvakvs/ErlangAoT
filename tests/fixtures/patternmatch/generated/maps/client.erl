-module(client).
-export([nested/2,retain/1]).
nested(K,V) -> answer:bound_key(K,answer:construct(K,V)).
retain(X) -> M = answer:independent(X), answer:literal(), answer:id(M).

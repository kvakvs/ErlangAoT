-module(client).
-export([nested/2,retain/1]).
nested(X,Y) -> answer:add(answer:divide(X,Y),X).
retain(X) -> A = answer:id(X), answer:constant(), answer:id(A).

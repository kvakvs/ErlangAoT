-module(client).
-export([nested/1,exhaust/1,body_failure/1,repeat/2]).
nested(0) -> first; nested(X) -> answer:local(X).
exhaust(X) -> answer:exhaust(X).
body_failure(X) -> answer:body_failure(X).
repeat(X,Y) -> answer:overlap(X,Y).

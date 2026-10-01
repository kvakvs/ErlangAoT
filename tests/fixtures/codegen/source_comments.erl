-module(source_comments).
-export([run/1, literal/0]).
-include("source_comments.hrl").
-define(INNER(X), helper(X)).
-define(OUTER(X), ?INNER(X)).
-file("logical-only.erl", 700).
run(Value) ->
    ?OUTER(
        Value).
literal() ->
    7. % original literal line

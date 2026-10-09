%% A declared argument type that shares no value with the entry domain contradicts the code.
%% error: argument.erl:5:8: argument 1 of specification for f can never be accepted: declared atom(), inferred integer()
-module(argument).
-export([f/1]).
-spec f(atom()) -> ok.
f(X) when is_integer(X) -> ok.

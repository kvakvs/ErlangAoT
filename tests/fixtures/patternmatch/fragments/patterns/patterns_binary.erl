-module(patterns_binary).
-export([do_basic_1/1]).
do_basic_1(<<Units:32, Payload:((Units - 1) * 4)/binary>>) ->
    Result = Payload,
    Result;
do_basic_1(Binary) when is_binary(Binary) -> no_match.

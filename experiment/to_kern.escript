#!/usr/bin/env escript
-module(to_kern).
-export([process/1, main/1]).

main(Args) ->
    [process(A) || A <- Args].

%% @doc Takes filename as input, produces compiled BEAM assembly AST and then
%% passes are invoked to process it further
-spec process(string()) -> #{}.
process(F) ->
  case compile:file(F, [to_kernel, binary, report]) of
    {ok, _ModuleName, Kern} ->
	io:format("~120p", [Kern]);
    Error ->
	io:format(standard_error, "~n~s: ~p~n", [F, Error])
end.

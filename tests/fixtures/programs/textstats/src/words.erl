%% Word and number helpers for the text statistics program.
-module(words).
-export([number/1, split/1, count/1]).

%% Parses digit- or minus-led arguments as integers; anything else is text.
number([C | _] = Arg) when C >= $0, C =< $9; C =:= $- ->
    try list_to_integer(Arg) of
        N -> {ok, N}
    catch
        error:badarg -> error
    end;
number(_) ->
    text.

%% Lowercases ASCII letters and splits on everything except letters and quotes.
split(Text) -> split(Text, [], []).

split([], [], Acc) ->
    lists:reverse(Acc);
split([], Word, Acc) ->
    lists:reverse([lists:reverse(Word) | Acc]);
split([C | Rest], Word, Acc) when C >= $A, C =< $Z ->
    split(Rest, [C + 32 | Word], Acc);
split([C | Rest], Word, Acc) when C >= $a, C =< $z; C =:= $' ->
    split(Rest, [C | Word], Acc);
split([_ | Rest], [], Acc) ->
    split(Rest, [], Acc);
split([_ | Rest], Word, Acc) ->
    split(Rest, [], [lists:reverse(Word) | Acc]).

%% Counts occurrences of each word in a map.
count(Words) ->
    lists:foldl(fun bump/2, #{}, Words).

bump(Word, Counts) ->
    case maps:find(Word, Counts) of
        {ok, N} -> Counts#{Word := N + 1};
        error -> Counts#{Word => 1}
    end.

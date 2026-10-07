%% Project-owned subset of the maps module (plan 11 step 39): original
%% implementations whose results and error reasons follow OTP's documented
%% behavior. Compiled from source into every program that calls it.
%% Iteration follows the map's key order (see docs/differences.md).
-module(maps).
-export([find/2, fold/3, from_list/1, get/2, keys/1, put/3, to_list/1, values/1]).

%% {ok, Value} for Key, or error.
find(Key, Map) when is_map(Map) ->
    case Map of
        #{Key := Value} -> {ok, Value};
        #{} -> error
    end;
find(_, Map) ->
    erlang:error({badmap, Map}).

%% Fun(Key, Value, Acc) over every association, in key order.
fold(Fun, Init, Map) when is_function(Fun, 3), is_map(Map) ->
    fold_pairs(Fun, Init, to_list(Map));
fold(Fun, _, Map) when is_function(Fun, 3) ->
    erlang:error({badmap, Map});
fold(_, _, _) ->
    erlang:error(badarg).

fold_pairs(Fun, Acc, [{Key, Value} | Rest]) -> fold_pairs(Fun, Fun(Key, Value, Acc), Rest);
fold_pairs(_, Acc, []) -> Acc.

%% A map of {Key, Value} pairs; a later pair replaces an earlier one with an equal key.
from_list(List) ->
    case pairs(List) of
        true -> #{Key => Value || {Key, Value} <- List};
        false -> erlang:error(badarg)
    end.

%% Whether a term is a proper list of two-element tuples.
pairs([{_, _} | Rest]) -> pairs(Rest);
pairs([]) -> true;
pairs(_) -> false.

%% The value of Key: {badkey, Key} when absent, {badmap, Map} for a non-map.
get(Key, Map) -> map_get(Key, Map).

%% The keys, in key order.
keys(Map) when is_map(Map) -> [Key || Key := _ <- Map];
keys(Map) -> erlang:error({badmap, Map}).

%% Map with Key associated to Value.
put(Key, Value, Map) -> Map#{Key => Value}.

%% The {Key, Value} pairs, in key order.
to_list(Map) when is_map(Map) -> [{Key, Value} || Key := Value <- Map];
to_list(Map) -> erlang:error({badmap, Map}).

%% The values, in key order.
values(Map) when is_map(Map) -> [Value || _ := Value <- Map];
values(Map) -> erlang:error({badmap, Map}).

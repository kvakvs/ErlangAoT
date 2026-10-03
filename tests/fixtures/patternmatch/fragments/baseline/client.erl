-module(client).
-export([id/1]).
id(Value) ->
    Saved = Value,
    Saved.

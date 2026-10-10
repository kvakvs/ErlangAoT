%% Exports one of the two required callbacks.
-module(partial).
-behaviour(shape).
-export([area/1]).

area(_) -> 0.

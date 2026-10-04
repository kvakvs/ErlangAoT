-module(checks).
-export([lookup/1, pick/1]).

%% Helpers whose results make the caller fail across a module boundary.
lookup(Key) -> {error, Key}.

pick(some) -> ok.

%% The same behaviour twice conflicts with itself.
-module(twice).
-behaviour(worker).
-behaviour(worker).
-export([init/1, info/0]).

init(_) -> ok.

info() -> none.

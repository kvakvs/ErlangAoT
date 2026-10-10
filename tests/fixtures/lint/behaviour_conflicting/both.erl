%% init/1 is required by both behaviours; info/0 is optional in server and not exported.
-module(both).
-behaviour(server).
-behaviour(worker).
-export([init/1, stop/0]).

init(_) -> ok.

stop() -> ok.

%% An exported optional callback counts as required by its behaviour.
-module(exported).
-behaviour(server).
-behaviour(worker).
-export([init/1, stop/0, info/0]).

init(_) -> ok.

stop() -> ok.

info() -> none.

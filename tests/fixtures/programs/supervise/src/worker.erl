%% Minimal worker: answers pings with a per-incarnation counter and exits on demand.
-module(worker).
-export([start_link/1, init/1]).

%% Spawns a linked worker and registers it under Name.
start_link(Name) ->
    Pid = spawn_link(worker, init, [Name]),
    true = register(Name, Pid),
    Pid.

%% Process entry point; counting restarts from zero on every start.
init(Name) -> loop(Name, 0).

loop(Name, Count) ->
    receive
        {ping, From} ->
            From ! {pong, Name, Count + 1},
            loop(Name, Count + 1);
        {crash, Reason} ->
            exit(Reason)
    end.

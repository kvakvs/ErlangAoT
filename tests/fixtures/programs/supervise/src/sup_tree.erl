%% One-for-one supervisor with transient children and a total restart limit.
%% Events go to an observer process so only the observer prints.
-module(sup_tree).
-export([start/3, init/3]).

%% Spawns the supervisor; it reports {sup, started, Names} once children run.
start(Names, MaxRestarts, Observer) ->
    spawn(sup_tree, init, [Names, MaxRestarts, Observer]).

%% Traps exits, starts every child and enters the supervision loop.
init(Names, MaxRestarts, Observer) ->
    process_flag(trap_exit, true),
    Children = maps:from_list([{worker:start_link(Name), Name} || Name <- Names]),
    Observer ! {sup, started, Names},
    loop(Children, 0, MaxRestarts, Observer).

%% Children maps pid to name; Restarts counts restarts so far.
loop(Children, Restarts, Max, Observer) ->
    receive
        {'EXIT', Pid, Reason} when is_map_key(Pid, Children) ->
            Name = map_get(Pid, Children),
            Rest = #{P => N || P := N <- Children, P =/= Pid},
            child_exit(Name, Reason, Rest, Restarts, Max, Observer);
        {which_children, From} ->
            From ! {children, lists:sort(maps:values(Children))},
            loop(Children, Restarts, Max, Observer)
    end.

%% Normal exits are not restarted; other exits restart until the limit is hit.
child_exit(Name, normal, Children, Restarts, Max, Observer) ->
    Observer ! {sup, exited, Name, normal, Restarts},
    loop(Children, Restarts, Max, Observer);
child_exit(Name, Reason, Children, Restarts, Max, Observer) when Restarts >= Max ->
    Observer ! {sup, gave_up, Name, Reason, Restarts},
    shutdown(Children, Observer),
    exit(shutdown);
child_exit(Name, Reason, Children, Restarts, Max, Observer) ->
    Pid = worker:start_link(Name),
    Observer ! {sup, restarted, Name, Reason, Restarts + 1},
    loop(Children#{Pid => Name}, Restarts + 1, Max, Observer).

%% Stops the remaining children and waits for each exit before reporting.
shutdown(Children, Observer) ->
    Pids = maps:keys(Children),
    [exit(Pid, shutdown) || Pid <- Pids],
    [
        receive
            {'EXIT', Pid, _} -> ok
        end
     || Pid <- Pids
    ],
    Observer ! {sup, stopped, lists:sort(maps:values(Children))}.

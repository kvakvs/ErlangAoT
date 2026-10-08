-module(crash_reports).
-export([main/1, id/1]).

id(X) -> X.

% Spin until a process has ended.
wait(Pid) ->
    case is_process_alive(Pid) of
        true -> wait(Pid);
        false -> ok
    end.

% Run Fun in a new process until it ends; only errors (and uncaught throws) write an error report.
run(Tag, Fun) ->
    wait(spawn(Fun)),
    io:format("~p ended~n", [Tag]).

main(_) ->
    run(returned, fun() -> done end),
    run(exit_normal, fun() -> exit(normal) end),
    run(exit_shutdown, fun() -> exit(shutdown) end),
    run(exit_kill, fun() -> exit(kill) end),
    run(exit_term, fun() -> exit({bye, [1, 2]}) end),
    run(caught, fun() ->
        try error(caught) of
            V -> V
        catch
            error:caught -> ok
        end
    end),
    run(error, fun() -> error(boom) end),
    run(throw, fun() -> throw(ball) end),
    run(badarith, fun() -> 1 / id(0) end),
    run(badmatch, fun() -> {ok, _} = id(nope) end),
    wait(spawn(crash_reports, missing, [1])),
    io:format("undef ended~n"),
    io:format("main done~n").

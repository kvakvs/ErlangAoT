-module(port_spawn).
-export([main/1]).

% Programs spawned as ports, here the Python helper.py run by the Python running the tests: line and packet
% framing, exit status, standard error, arguments and environment, a large write, closing, owner ends, errors and
% os:cmd/1.

python() -> os:getenv("CLAUSE_TEST_PYTHON").

% A port running helper.py in Mode.
helper(Mode, Args, Options) ->
    open_port({spawn_executable, python()}, [{args, ["helper.py", Mode | Args]} | Options]).

% Messages of P until {P, eof}, its exit signal or a timeout.
messages(P) ->
    receive
        {P, eof} -> [eof];
        {P, Message} -> [Message | messages(P)];
        {'EXIT', P, Reason} -> [{exit, Reason}]
    after 10000 -> [timeout]
    end.

% The next message of P.
next(P) ->
    receive
        {P, Message} -> Message
    after 10000 -> timeout
    end.

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

echo() ->
    P = helper("echo", [], [{line, 80}]),
    port_command(P, "hello\n"),
    port_command(P, ["io", [<<"list">>], $\n]),
    First = next(P),
    Second = next(P),
    io:format("echo ~p ~p~n", [First, Second]),
    io:format("os_pid ~p~n", [is_integer(element(2, erlang:port_info(P, os_pid)))]),
    port_close(P),
    io:format("closed ~p~n", [messages(P)]).

framing() ->
    io:format("lines ~p~n", [messages(helper("lines", [], [{line, 6}, eof]))]),
    io:format("exit ~p~n", [messages(helper("exit", ["7"], [exit_status]))]),
    io:format("exit 0 ~p~n", [messages(helper("exit", ["0"], []))]),
    P = helper("packet", [], [{packet, 2}, binary]),
    port_command(P, <<"abc">>),
    P ! {self(), {command, [<<"de">>, "f"]}},
    Replies = [next(P), next(P)],
    P ! {self(), close},
    io:format("packet ~p ~p~n", [Replies, messages(P)]).

environment() ->
    io:format("streams ~p~n", [messages(helper("streams", [], [{line, 80}, stderr_to_stdout, eof]))]),
    Env = [{"CLAUSE_PROBE", "probe value"}, {"CLAUSE_UNSET", false}],
    Info = helper("info", ["a b", "c\"d", "e\\"], [{line, 80}, eof, {env, Env}]),
    io:format("info ~p~n", [messages(Info)]),
    Bytes = 200000,
    Count = helper("count", [integer_to_list(Bytes)], [{line, 80}, eof]),
    Chunk = iolist_to_binary([$x || _ <- lists:seq(1, Bytes div 4)]),
    [port_command(Count, Chunk) || _ <- lists:seq(1, 4)],
    io:format("count ~p~n", [messages(Count)]).

owners() ->
    Self = self(),
    Owner = spawn(fun() ->
        Self ! {port, helper("echo", [], [{line, 80}])},
        receive
            stop -> ok
        end
    end),
    P =
        receive
            {port, Q} -> Q
        end,
    Ref = monitor(port, P),
    Owner ! stop,
    receive
        {'DOWN', Ref, port, P, Reason} -> io:format("owner ended ~p~n", [Reason])
    end.

errors() ->
    io:format("~p~n", [
        [
            error_of(fun() -> open_port({spawn_executable, "no_such_program_57d"}, []) end),
            error_of(fun() -> open_port({spawn_executable, python()}, [{args, [1]}]) end),
            error_of(fun() -> open_port({spawn, 1}, []) end)
        ]
    ]),
    io:format("os:cmd ~p~n", [[C || C <- os:cmd("echo hello"), C =/= $\r]]).

main(_) ->
    process_flag(trap_exit, true),
    echo(),
    framing(),
    environment(),
    owners(),
    errors(),
    io:format("done~n").

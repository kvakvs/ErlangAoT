-module(processes).
-export([main/1, count/2, sum/1, id/1]).

% Spin until a process has ended; without preemption the awaited process would never run.
wait(Pid) ->
    case is_process_alive(Pid) of
        true -> wait(Pid);
        false -> ok
    end.

wait_all([]) ->
    ok;
wait_all([Pid | Rest]) ->
    wait(Pid),
    wait_all(Rest).

% A busy loop of N calls that prints its tag at the end.
count(0, Tag) -> io:format("~p done~n", [Tag]);
count(N, Tag) -> count(N - 1, Tag).

sum(List) -> sum(List, 0).

sum([], Acc) -> Acc;
sum([H | T], Acc) -> sum(T, Acc + H).

fib(N) when N < 2 -> N;
fib(N) -> fib(N - 1) + fib(N - 2).

error_of(F) ->
    try F() of
        V -> {ok, V}
    catch
        error:R -> {error, R}
    end.

id(X) -> X.

main(["crash"]) ->
    % A crashing process ends alone; its spawner and other processes go on.
    Crash = spawn(fun() -> error(boom) end),
    Undefined = spawn(processes, missing, [1]),
    Arity = spawn(processes:id(fun(X) -> X end)),
    wait_all([Crash, Undefined, Arity]),
    io:format("~p~n", [[is_process_alive(P) || P <- [Crash, Undefined, Arity]]]),
    Survivor = spawn(processes, count, [1000, survivor]),
    wait(Survivor),
    io:format("main survived~n");
main([]) ->
    Main = self(),
    % The process spawned first spins until the second one ends, so the second must run in between.
    Short = spawn(processes, count, [20000, short]),
    Long = spawn(fun() ->
        wait(Short),
        io:format("long saw short end~n"),
        count(20000, long)
    end),
    wait(Long),
    io:format("~p~n", [[is_process_alive(Short), is_process_alive(Long), is_process_alive(Main)]]),
    % Ten thousand processes, each with its own heap.
    Pids = [spawn(fun() -> fib(10) end) || _ <- lists:seq(1, 10000)],
    wait_all(Pids),
    io:format("~p processes ended, ~p alive~n", [
        length(Pids), length([P || P <- Pids, is_process_alive(P)])
    ]),
    % Arguments and captured values are copied into the new process.
    List = lists:seq(1, 1000),
    Copy = spawn(fun() -> io:format("sum ~p, self differs ~p~n", [sum(List), self() =/= Main]) end),
    wait(Copy),
    Args = spawn(processes, count, [3, {args, List =:= lists:seq(1, 1000)}]),
    wait(Args),
    io:format("~p~n", [
        [
            error_of(fun() -> spawn(id(42)) end),
            error_of(fun() -> spawn(id(processes), id("count"), []) end),
            error_of(fun() -> spawn(id(processes), count, [1 | id(2)]) end),
            error_of(fun() -> is_process_alive(id(self)) end),
            is_pid(spawn(fun() -> ok end))
        ]
    ]),
    % A process still running when the main process returns does not keep the program alive.
    spawn(fun() -> count(100000000, forever) end),
    io:format("main done~n").

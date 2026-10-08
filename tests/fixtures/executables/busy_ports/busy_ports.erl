-module(busy_ports).
-export([main/1]).

% A spawned program that reads nothing for a second makes its port busy once output queues up: port_command/3
% with nosuspend answers false, force is not supported by the driver, port_command/2 suspends its caller until the
% program has read enough, and nothing written is lost.

-define(CHUNK, 65536).

main(_) ->
    Port = open_port({spawn_executable, os:getenv("CLAUSE_TEST_PYTHON")}, [
        {args, ["helper.py"]}, {packet, 4}, binary
    ]),
    Chunk = <<0:(?CHUNK * 8)>>,
    Sent = fill(Port, Chunk, 0),
    io:format("busy after writing ~p~n", [Sent > 0]),
    {queue_size, Queued} = erlang:port_info(Port, queue_size),
    io:format("queued ~p~n", [Queued > 0]),
    io:format("force ~p~n", [force(Port, Chunk)]),
    Self = self(),
    Writer = spawn(fun() ->
        true = port_command(Port, Chunk),
        Self ! {written, self()}
    end),
    Early =
        receive
            {written, Writer} -> written
        after 200 -> suspended
        end,
    io:format("writer ~p~n", [Early]),
    % The writer resumes once the program has read enough; only then does the last packet follow its chunk.
    receive
        {written, Writer} -> ok
    end,
    true = port_command(Port, <<"end">>),
    receive
        {Port, {data, Count}} ->
            io:format("received all ~p~n", [
                list_to_integer(binary_to_list(Count)) =:= (Sent + 1) * ?CHUNK
            ])
    end,
    port_close(Port).

% Write chunks without suspending until the port is busy; the number written.
fill(Port, Chunk, Written) ->
    case port_command(Port, Chunk, [nosuspend]) of
        true -> fill(Port, Chunk, Written + 1);
        false -> Written
    end.

% What a forced write raises.
force(Port, Chunk) ->
    try port_command(Port, Chunk, [force]) of
        Result -> Result
    catch
        error:Reason -> Reason
    end.

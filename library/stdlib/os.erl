-module(os).
-export([cmd/1]).

% The project-owned os module (docs/library.md): os:cmd/1 in Erlang over a port; os:type/0 and os:getenv/1 are
% builtins of the runtime.

% Run Command through the system's command processor (cmd.exe /c on Windows, /bin/sh -c elsewhere) and return
% what it writes to its standard output and error, as bytes.
cmd(Command) when is_atom(Command) ->
    cmd(atom_to_list(Command));
cmd(Command) when is_list(Command) ->
    Port = open_port(shell(os:type(), Command), [binary, stderr_to_stdout, stream, in, hide]),
    Monitor = monitor(port, Port),
    Bytes = collect(Port, Monitor, []),
    % The port was linked to the caller: drop its exit signal when the caller traps exits.
    receive
        {'EXIT', Port, _} -> ok
    after 0 -> ok
    end,
    binary_to_list(Bytes).

% The port name that runs Command.
shell({win32, _}, Command) ->
    Processor =
        case os:getenv("COMSPEC") of
            false -> "cmd";
            Path -> Path
        end,
    {spawn, Processor ++ " /c" ++ Command};
shell(_, Command) ->
    {spawn, Command}.

% The output of Port until it closed.
collect(Port, Monitor, Parts) ->
    receive
        {Port, {data, Bytes}} -> collect(Port, Monitor, [Bytes | Parts]);
        {'DOWN', Monitor, port, Port, _} -> iolist_to_binary(lists:reverse(Parts))
    end.

-module(inet).
-export([port/1, sockname/1, peername/1, setopts/2, close/1]).

% The project-owned subset of inet (docs/library.md, docs/ports.md#sockets) for sockets of gen_tcp and gen_udp.

% The local port number of Socket.
port(Socket) ->
    case sockname(Socket) of
        {ok, {_, Port}} -> {ok, Port};
        Error -> Error
    end.

sockname(Socket) -> erlang_aot_socket:names(Socket, false).

peername(Socket) -> erlang_aot_socket:names(Socket, true).

% Change active, packet and binary or list; other options are accepted and not applied.
setopts(Socket, Options) -> erlang_aot_socket:setopts(Socket, Options).

close(Socket) -> erlang_aot_socket:close(Socket).

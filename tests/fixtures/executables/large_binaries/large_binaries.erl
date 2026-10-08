-module(large_binaries).
-export([main/1]).

%% Binaries have no size cap beyond an optional process heap budget: these
%% exceed the 1,000,000 bits Clause used to allow per value.
main([]) ->
    Big = double(<<1, 0:65528>>, 7),
    show(byte_size(Big)),
    <<Head, _/binary>> = Big,
    show(Head),
    show(edges(<<-1:1000008>>)),
    show(edges(<<-2:1000008/little>>)),
    show(edges(<<5:1000008>>)).

show(Value) -> erlang:display(Value).

%% Concatenate a binary with itself N times.
double(Binary, 0) -> Binary;
double(Binary, N) -> double(<<Binary/binary, Binary/binary>>, N - 1).

%% Size, first byte and last byte.
edges(Binary) ->
    Skip = byte_size(Binary) - 1,
    <<First, _/binary>> = Binary,
    <<_:Skip/binary, Last>> = Binary,
    {byte_size(Binary), First, Last}.

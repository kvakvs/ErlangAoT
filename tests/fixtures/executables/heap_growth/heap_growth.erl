-module(heap_growth).
-export([main/1]).

%% A process has no memory cap by default: it keeps 1,100 off-heap binaries of
%% 65,540 bytes (72 MB, more than the 64 MiB budget processes used to have)
%% while dropping four more per step, so collections keep it near its live
%% size.
main([]) ->
    show(len(grow(id(1100), <<0:524288>>, []))).

show(Value) -> erlang:display(Value).

id(Value) -> Value.

%% A tail loop keeping one binary per step and dropping four.
grow(0, _, Kept) ->
    Kept;
grow(N, Base, Kept) ->
    drop(4, Base),
    grow(N - 1, Base, [<<N:32, Base/binary>> | Kept]).

drop(0, _) ->
    ok;
drop(N, Base) ->
    <<_:32, _/binary>> = <<N:32, Base/binary>>,
    drop(N - 1, Base).

len(List) -> len(List, 0).

len([], Count) -> Count;
len([_ | Tail], Count) -> len(Tail, Count + 1).

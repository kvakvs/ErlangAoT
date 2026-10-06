-module(heap_exhaustion).
-export([main/1]).

%% Both runs retain one 65,540-byte off-heap binary per step and drop four
%% more. The 64 MiB process budget holds about 1,020 of them on every word
%% width.

%% The first argument selects a scenario.
main(["fits"]) ->
    %% 900 binaries keep 88% of the budget live, so the run only finishes when
    %% the dropped binaries are collected before the budget runs out.
    show(len(grow(id(900), <<0:524288>>, [])));
main(["exceeds"]) ->
    %% 2,500 binaries do not fit, so ErlangAoT stops the program; OTP has no
    %% default heap limit and prints 2500.
    show(growing),
    show(len(grow(id(2500), <<0:524288>>, []))).

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

-module(expressions).
-export([literals/0, operators/2, calls/1, containers/2, records/1, funs/1, binaries/1]).

-record(r, {a = 1, b}).
-record(#nr{x = 0, y = 1}).

literals() ->
    {ok, 'Quoted atom', 42, -7, 16#ff, 1.5, 2.0e10, $a, $\n, "string", "adjacent" " strings", [],
        'λ', "unicodé"}.

operators(A, B) ->
    C = A + B * 2 - (A div B) rem 3,
    D = -A,
    E = not (A > B) andalso B =/= 0 orelse A =:= B,
    F = A band B bor (A bxor B) bsl 1 bsr 2,
    G = [1] ++ [2] -- [3],
    self() ! {C, D, E, F, G},
    (A) = A,
    X = Y = {A, B},
    {X, Y, bnot A, +B, A / B, A == B, A /= B, A =< B, A >= B, A < B, A or B, A xor B, A and B}.

calls(M) ->
    local(1),
    M:remote(2),
    lists:reverse([3]),
    (fun local/1)(4),
    M:(name())(5),
    erlang:apply(M, f, []),
    catch local(6).

local(X) -> X.
name() -> f.

containers(L, Map) ->
    T = {},
    T1 = {a, {b, c}},
    L1 = [a, b | L],
    L2 = [x | [y | []]],
    M1 = #{a => 1, {b} => [2]},
    M2 = Map#{a := 2, c => 3},
    M3 = (M2)#{d => 4},
    #{a := V} = M1,
    {T, T1, L1, L2, M1, M2, M3, V}.

records(R) ->
    R1 = #r{a = 2},
    R2 = R#r{b = 3},
    A = R#r.a,
    I = #r.b,
    #r{a = Pa} = R1,
    N = #nr{x = 1},
    N1 = N#nr{y = 2},
    Nx = N1#nr.x,
    Q = #expressions:nr{x = 5},
    Any = #_{x = 6},
    W = #r{_ = undefined},
    {R1, R2, A, I, Pa, Nx, Q, Any, W}.

funs(M) ->
    F1 = fun local/1,
    F2 = fun lists:reverse/1,
    F3 = fun M:local/1,
    F4 = fun
        (X) when X > 0 -> X;
        (_) -> 0
    end,
    F5 = fun
        Fact(0) -> 1;
        Fact(N) -> N * Fact(N - 1)
    end,
    F6 = fun() -> ok end,
    {F1, F2, F3, F4, F5, F6}.

binaries(X) ->
    B1 = <<>>,
    B2 = <<1, 2, 3>>,
    B3 = <<X:8/integer-little, "abc", (X + 1):16/unit:1, 3.5/float>>,
    <<Head:4, Rest/bitstring>> = B3,
    B4 = <<-1:8/signed>>,
    {B1, B2, B3, Head, Rest, B4, ~"sigil", ~b"bin"}.

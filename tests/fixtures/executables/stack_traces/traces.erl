-module(traces).
-export([main/1]).
-compile(nowarn_deprecated_catch).

%% The first argument selects a scenario. Frames print as {Module, Function, Arity}
%% because source locations and the frames below main/1 differ from OTP. Every
%% traced function can also return, otherwise OTP turns calls to it into tail calls.
main(["classes"]) ->
    show(level1(ok)),
    show(
        try
            {w, level1(error)}
        catch
            error:E:SE -> {error, E, frames(4, SE)}
        end
    ),
    show(
        try
            {w, level1(exit)}
        catch
            exit:X:SX -> {exit, X, frames(4, SX)}
        end
    ),
    show(
        try
            {w, level1(throw)}
        catch
            T:SV:ST -> {T, SV, frames(4, ST)}
        end
    ),
    show(tracer:outer(ok)),
    show(
        try
            {w, tracer:outer(error)}
        catch
            _:R:SR -> {remote, R, frames(3, SR)}
        end
    );
main(["catch"]) ->
    show(caught(4, catch {w, level1(error)})),
    show(caught(4, catch {w, level1(exit)})),
    show(caught(4, catch {w, level1(throw)})),
    show(caught(3, catch {w, tracer:outer(error)})),
    show(caught(1, catch {w, arguments(list, [c])}));
main(["runtime"]) ->
    show(runtime(ok)),
    show(runtime(badmatch)),
    show(runtime(case_clause)),
    show(runtime(if_clause)),
    show(runtime(function_clause)),
    show(runtime(badarith)),
    show(runtime(try_clause));
main(["depth"]) ->
    show(d1(ok)),
    show(
        try
            {w, d1(error)}
        catch
            error:R:S -> {R, depth(S), frames(8, S)}
        end
    );
main(["arguments"]) ->
    show(arguments(ok, [])),
    show(top(list, [1, two, "three"])),
    show(top(list, [])),
    show(top(list, [a | b])),
    show(top(none, [1])),
    show(top(other, [1])),
    show(top(info, [x]));
main(["rethrow"]) ->
    show(rethrow(ok)),
    show(trace(unmatched)),
    show(trace(protected)),
    show(trace(raise)),
    show(
        try
            {w, nested()}
        catch
            C:R:S -> {C, R, frames(4, S)}
        end
    ),
    show(same(error)),
    show(same(throw));
main(["raise"]) ->
    show(
        try
            erlang:raise(throw, given, [{m, f, 1}, {m, g, [a], [{line, 3}]}])
        catch
            throw:G:SG -> {G, SG}
        end
    ),
    show(
        try
            erlang:raise(exit, empty, [])
        catch
            exit:Em:SEm -> {Em, SEm}
        end
    ),
    show(
        try
            erlang:raise(error, long, long_stack())
        catch
            error:L:SL -> {L, depth(SL), SL}
        end
    ),
    show(
        try
            erlang:raise(error, improper, [{m, f, 1, [a | b]}])
        catch
            error:I:SI -> {I, SI}
        end
    );
main(["malformed"]) ->
    show(erlang:raise(foo, x, [])),
    show(erlang:raise(error, x, bad)),
    show(erlang:raise(exit, x, [{m, f, 1} | z])),
    show(erlang:raise(throw, x, [x])),
    show(erlang:raise(error, x, [{m, f}])),
    show(erlang:raise(error, x, [{m, f, 1, [], extra}])),
    show(erlang:raise(error, x, [{1, f, 1}])),
    show(erlang:raise(error, x, [{m, "f", 1, []}])),
    show(erlang:raise(error, x, [{m, f, 1, loc}])),
    show(erlang:raise(error, x, [{m, f, 1}, bad]));
main(["uncaught"]) ->
    erlang:raise(exit, gone, [{m, f, 0}]).

show(Value) -> erlang:display(Value).

%% A three-level chain raising each class.
level1(Kind) -> {level1, level2(Kind)}.
level2(Kind) -> {level2, level3(Kind)}.
level3(error) -> error(boom);
level3(exit) -> exit(stop);
level3(throw) -> throw(ball);
level3(Kind) -> Kind.

%% Typed runtime errors; OTP adds a frame for a failing operator, which frames/2 skips.
runtime(Kind) ->
    try
        {w, call(Kind)}
    catch
        error:R:S -> {R, frames(3, S)}
    end.

call(Kind) -> {call, failing(Kind)}.

failing(badmatch) ->
    {ok, V} = id(error),
    V;
failing(case_clause) ->
    case id(9) of
        0 -> zero
    end;
failing(if_clause) ->
    N = id(1),
    if
        N > 5 -> big
    end;
failing(function_clause) ->
    {pick, pick(id(c))};
failing(badarith) ->
    id(a) + 1;
failing(try_clause) ->
    try id(7) of
        0 -> zero
    catch
        _ -> caught
    end;
failing(Kind) ->
    Kind.

pick(a) -> first;
pick(b) -> second.

id(Value) -> Value.

%% Ten nested frames; traces keep the innermost eight.
d1(K) -> {d1, d2(K)}.
d2(K) -> {d2, d3(K)}.
d3(K) -> {d3, d4(K)}.
d4(K) -> {d4, d5(K)}.
d5(K) -> {d5, d6(K)}.
d6(K) -> {d6, d7(K)}.
d7(K) -> {d7, d8(K)}.
d8(K) -> {d8, d9(K)}.
d9(K) -> {d9, d10(K)}.
d10(error) -> error(deep);
d10(K) -> K.

%% erlang:error/2,3 shows a list of arguments in the top frame instead of the arity.
arguments(list, Args) -> {arguments, error(why, Args)};
arguments(none, _) -> {arguments, error(why, none)};
arguments(other, _) -> {arguments, error(why, other)};
arguments(info, Args) -> {arguments, error(why, Args, [{error_info, #{cause => test}}])};
arguments(Kind, _) -> Kind.

top(Kind, Args) ->
    try
        {w, arguments(Kind, Args)}
    catch
        error:R:S -> {R, top_frame(S)}
    end.

top_frame([{M, F, A, Location} | _]) when is_list(Location) -> {M, F, A}.

%% Re-raised exceptions keep the stack of their original raise.
rethrow(unmatched) ->
    try
        {w, level1(error)}
    catch
        throw:never -> never
    end;
rethrow(protected) ->
    try
        {w, level1(exit)}
    after
        show(cleanup)
    end;
rethrow(raise) ->
    try
        {w, level1(error)}
    catch
        error:R:S -> erlang:raise(exit, {again, R}, S)
    end;
rethrow(Kind) ->
    Kind.

trace(Kind) ->
    try
        {w, rethrow(Kind)}
    catch
        C:R:S -> {C, R, frames(4, S)}
    end.

nested() ->
    try
        {w, rethrow(unmatched)}
    catch
        error:never -> never
    end.

%% A stack raised again with raise/3 is caught unchanged.
same(Class) ->
    try
        {w, level1(Class)}
    catch
        C:R:S ->
            try
                erlang:raise(C, R, S)
            catch
                C:R:S2 -> {C, S =:= S2}
            end
    end.

caught(N, {'EXIT', {Reason, Stack}}) when is_list(Stack) -> {'EXIT', Reason, frames(N, Stack)};
caught(_, Other) -> Other.

%% A raise/3 stack longer than the trace limit.
long_stack() ->
    [
        {m, f1, 1},
        {m, f2, 2},
        {m, f3, 3},
        {m, f4, 4},
        {m, f5, 5},
        {m, f6, 6},
        {m, f7, 7},
        {m, f8, 8},
        {m, f9, 9},
        {m, f10, 10}
    ].

depth(S) when length(S) =:= 8 -> eight;
depth(S) when is_list(S) -> other.

%% The first N frames of a stack trace.
frames(N, Stack) -> first(N, user(Stack)).

user([{erlang, _, _, _} | Stack]) -> Stack;
user(Stack) -> Stack.

first(1, [F1 | _]) ->
    [frame(F1)];
first(3, [F1, F2, F3 | _]) ->
    [frame(F1), frame(F2), frame(F3)];
first(4, [F1, F2, F3, F4 | _]) ->
    [frame(F1), frame(F2), frame(F3), frame(F4)];
first(8, [F1, F2, F3, F4, F5, F6, F7, F8 | _]) ->
    [frame(F1), frame(F2), frame(F3), frame(F4), frame(F5), frame(F6), frame(F7), frame(F8)].

%% OTP shows the arguments of a function_clause frame; ErlangAoT shows its arity.
frame({M, F, A, Location}) when is_list(Location) -> {M, F, arity(A)}.

arity(A) when is_integer(A) -> A;
arity([_]) -> 1.

-module(catches).
-export([main/1]).
-compile(nowarn_deprecated_catch).

-record(pt, {x, y}).

%% The first argument selects a scenario; every caught value is displayed with its stack hidden.
main(["classes"]) ->
    show(catch throw(ball)),
    show(catch throw({ball, [1, 2]})),
    show(catch exit(shutdown)),
    show(catch exit(normal)),
    show(catch erlang:exit({stop, 1})),
    show(catch error(oops)),
    show(catch erlang:error(with_args, [a])),
    show(catch error({big, 123456789012345678901234567890})),
    show(catch id(plain)),
    show(catch 42);
main(["runtime"]) ->
    show(catch match(id(1), id(2))),
    show(catch clause(id(other))),
    show(catch add(id(1), id(a))),
    show(catch divide(id(1), id(0))),
    show(catch select(id(3))),
    show(catch choose(id(0))),
    show(catch both(id(1))),
    show(catch update(id(#{}))),
    show(catch update(id(nomap))),
    show(catch field(id({other, 1, 2}))),
    show(catch field(id(#pt{x = 1, y = 2})));
main(["nested"]) ->
    show(catch catch throw(inner)),
    show(catch {outer, catch throw(inner)}),
    show(catch {outer, catch exit(inner), throw(outer)}),
    show(catch (catch error(first)) =:= x),
    show(
        catch begin
            X = (catch throw(1)),
            throw({X, 2})
        end
    ),
    show(catch thrower:rethrow(id(deep))),
    show(catch thrower:guarded(id(fine))),
    show(catch thrower:guarded(id(bad)));
main(["flow"]) ->
    Caught = (catch thrower:depth(3)),
    show(Caught),
    show(
        case catch thrower:depth(0) of
            {'EXIT', _} -> exited;
            Other -> {value, Other}
        end
    ),
    show([catch throw(a), catch exit(b), catch c]),
    show(branch(id(1))),
    show(branch(id(2))),
    Z = id(5),
    show(catch Z + a),
    show(catch (Z = 5)),
    show(catch (Z = 6)),
    show({Z, after_catches});
main(["uncaught"]) ->
    show(catch throw(first)),
    show(catch thrower:depth(2)),
    thrower:depth(1);
main(["halt"]) ->
    show(catch erlang:halt(3)),
    erlang:display(never);
main(Args) ->
    erlang:display(Args).

%% Hide the stack of an error, which ErlangAoT leaves empty until stack traces exist.
show({'EXIT', {Reason, Stack}}) when is_list(Stack) -> erlang:display({'EXIT', {Reason, stack}});
show(Value) -> erlang:display(Value).

%% Opaque values keep the OTP compiler from folding the failing operations below.
id(X) -> X.

match(A, B) ->
    A = B.

clause(one) -> one.

add(A, B) -> A + B.

divide(A, B) -> A div B.

select(X) ->
    case X of
        1 -> one
    end.

choose(X) ->
    if
        X > 0 -> positive
    end.

both(X) -> X andalso true.

update(M) -> M#{k := 1}.

field(P) -> P#pt.x.

%% A variable bound outside the catch by the enclosing match is exported from each case clause.
branch(X) ->
    case X of
        1 -> Y = (catch throw(one));
        _ -> Y = (catch exit(two))
    end,
    {branch, Y}.

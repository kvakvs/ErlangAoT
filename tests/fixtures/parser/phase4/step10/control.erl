-module(control).
-define(ID(X), X).
f(X) -> begin
    ?ID(begin a, b end),
    ?ID(case X of {a,Y} when Y > 0, is_integer(Y); Y =:= 0 -> Y, done; _ -> other end),
    ?ID(if X -> begin yes, ok end; true; false -> no end),
    ?ID(receive {a,Y} when is_integer(Y) -> Y; stop -> done end),
    ?ID(receive X -> X after 10 + 1 -> timeout, done end),
    ?ID(receive after begin a, 0 end -> timeout end),
    case X of f() -> accepted_but_not_a_pattern end
end.
g() -> {begin ok end, (case x of x -> yes end)(x), (if true -> #{} end)#{a=>1}}.

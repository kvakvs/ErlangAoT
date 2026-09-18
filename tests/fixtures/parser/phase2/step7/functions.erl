-module(functions).
-define(P, {tag, X}).
f(?P, [H|T]) when is_integer(H), H > 0; X == fallback -> A = H + 1, {A,T};
f(_, []) -> empty.
zero() when true; false -> a, b;
zero() -> c.
'when'(A = B = C, +1, -2, bnot 3, not true, (X < Y), "a" "b", ~b"λ") -> {A,B,C,X,Y}.
permissive({f(), A ! B, M:F}, [g()|h()], {catch f()}, [A andalso B]) -> Unbound.
guards(X) when custom(X), X = 1; X ! message -> ok.
duplicate(X) -> X.
duplicate(X) -> X.
guard_alternative(X) when true; f(Y) -> ok.

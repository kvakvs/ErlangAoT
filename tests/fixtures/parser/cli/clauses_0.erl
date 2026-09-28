-define(P, {tag, X}).
f(?P) when is_integer(X), X > 0; X == fallback -> A = X, A;
f(_) -> empty.

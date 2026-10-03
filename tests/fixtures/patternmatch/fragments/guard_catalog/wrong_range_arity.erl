-module(wrong_range_arity).
% Guard catalog semantic case.
f(X) when is_integer(X, X) -> X.

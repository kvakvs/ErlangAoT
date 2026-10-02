-module(quoted_operator).
% Guard catalog semantic case.
f(X) when '+'(X,1) -> X.

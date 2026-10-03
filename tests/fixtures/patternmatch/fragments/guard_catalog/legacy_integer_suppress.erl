-module(legacy_integer_suppress).
% Guard catalog semantic case.
-compile({no_auto_import, [is_integer/1]}).
f(X) when integer(X) -> X.

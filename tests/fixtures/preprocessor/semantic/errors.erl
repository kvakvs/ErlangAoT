-module(errors).
-define(A,1).
-define(A,2).
-define(F(X,X), X).
-define(R,?R).
cycle() -> ?R.
missing() -> ?NO.
-undef(A).
missing_again() -> ?A.
-else.
-endif.
-if(true orelse not_a_guard()).
skip() -> wrong.
-endif.
-warning({warning, [1,2]}).
-error(problem).
-warning(1+2).
-include("missing.hrl").
good() -> ok.

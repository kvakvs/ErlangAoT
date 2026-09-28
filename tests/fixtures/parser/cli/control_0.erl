-define(C(X), case X of f() when true; false -> begin a,b end end).
f() -> ?C(x).
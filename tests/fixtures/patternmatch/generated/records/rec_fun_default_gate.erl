-module(rec_fun_default_gate).
% Record semantic fixture.
-record(r,{a=fun(X)->X end}). f() -> #r{}.

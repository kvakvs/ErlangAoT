-module(rec_default_forward).
% Record semantic fixture.
-record(r,{a=#s{}}). -record(s,{b}). f() -> #r{}.

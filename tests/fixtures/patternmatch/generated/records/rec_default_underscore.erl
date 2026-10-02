-module(rec_default_underscore).
% Record semantic fixture.
-record(r,{a=(_=1)}). f() -> #r{}.

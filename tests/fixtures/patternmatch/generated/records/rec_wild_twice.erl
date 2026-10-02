-module(rec_wild_twice).
% Record semantic fixture.
-record(r,{a,b}). f() -> #r{_=1,_=2}.

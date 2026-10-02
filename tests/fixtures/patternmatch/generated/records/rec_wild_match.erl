-module(rec_wild_match).
% Record semantic fixture.
-record(r,{a,b}). f(#r{_=X}) -> X.

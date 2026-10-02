-module(rec_guard_bignum).
% Record semantic fixture.
f(X) when is_record(X,r,1267650600228229401496703205376) -> ok; f(_) -> no.

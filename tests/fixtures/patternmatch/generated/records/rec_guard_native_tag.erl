-module(rec_guard_native_tag).
% Record semantic fixture.
f(X) when erlang:is_record(X,r,atom) -> ok; f(_) -> no.

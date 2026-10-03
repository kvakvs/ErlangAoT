-module(gate_is_record_1_1_0).
% Guard catalog semantic case.
f(X) when erlang:is_record(X) -> X.

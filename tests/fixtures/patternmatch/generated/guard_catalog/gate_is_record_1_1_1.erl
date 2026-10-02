-module(gate_is_record_1_1_1).
% Guard catalog semantic case.
f(X) when true orelse erlang:is_record(X) -> X.

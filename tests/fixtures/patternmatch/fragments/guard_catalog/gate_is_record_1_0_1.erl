-module(gate_is_record_1_0_1).
% Guard catalog semantic case.
f(X) when true orelse is_record(X) -> X.

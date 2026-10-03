-module(gate_is_record_1_0_0).
% Guard catalog semantic case.
f(X) when is_record(X) -> X.

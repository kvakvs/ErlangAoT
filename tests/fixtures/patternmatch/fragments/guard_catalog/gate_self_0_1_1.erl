-module(gate_self_0_1_1).
% Guard catalog semantic case.
f(X) when true orelse erlang:self() -> X.

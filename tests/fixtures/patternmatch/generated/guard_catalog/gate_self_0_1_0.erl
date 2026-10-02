-module(gate_self_0_1_0).
% Guard catalog semantic case.
f(X) when erlang:self() -> X.

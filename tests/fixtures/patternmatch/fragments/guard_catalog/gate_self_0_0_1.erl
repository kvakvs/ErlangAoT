-module(gate_self_0_0_1).
% Guard catalog semantic case.
f(X) when true orelse self() -> X.

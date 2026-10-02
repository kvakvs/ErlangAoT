-module(gate_node_1_0_1).
% Guard catalog semantic case.
f(X) when true orelse node(X) -> X.

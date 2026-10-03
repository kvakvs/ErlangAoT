-module(gate_node_1_0_0).
% Guard catalog semantic case.
f(X) when node(X) -> X.

-module(gate_node_0_1_1).
% Guard catalog semantic case.
f(X) when true orelse erlang:node() -> X.

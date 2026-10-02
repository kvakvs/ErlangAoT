-module(gate_node_1_1_1).
% Guard catalog semantic case.
f(X) when true orelse erlang:node(X) -> X.

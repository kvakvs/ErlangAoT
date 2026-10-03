-module(gate_node_1_1_0).
% Guard catalog semantic case.
f(X) when erlang:node(X) -> X.

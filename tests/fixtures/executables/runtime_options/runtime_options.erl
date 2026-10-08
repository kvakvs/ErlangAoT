-module(runtime_options).
-export([main/1]).

%% Prints the arguments main/1 receives. Clause takes leading runtime
%% options such as --max-atoms out of them, and reads CLAUSE_FLAGS first.
main(Args) -> erlang:display(Args).

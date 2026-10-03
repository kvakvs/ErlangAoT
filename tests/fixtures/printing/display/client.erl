-module(client).
-export([show/1]).

%% Reaches display through a remote call into another module of the batch.
show(Term) -> answer:show(Term).

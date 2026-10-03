%% Second module exporting main/1, making automatic entry detection ambiguous.
-module(other).
-export([main/1]).

main(_Args) -> ok.

-module(expanded_attributes).
-include("attributes.hrl").
-define(EXPORTS, [f/0]).
-export(?EXPORTS).
f() -> #included{}.

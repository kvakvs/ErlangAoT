-module(shapes).
-export([area/1, describe/1]).

%% Helpers returning tagged results that the caller selects on across a module boundary.
area({square, Side}) -> {ok, Side * Side};
area(circle) -> {error, unsupported}.

describe(Area) ->
    case Area of
        16 -> sixteen;
        _ -> {area, Area}
    end.

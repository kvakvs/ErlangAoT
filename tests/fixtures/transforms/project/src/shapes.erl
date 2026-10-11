-module(shapes).
-export([area/1]).
-compile({parse_transform, pt_rewrite}).

area(Side) ->
    double(Side) * Side.

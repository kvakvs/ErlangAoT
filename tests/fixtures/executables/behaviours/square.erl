%% Uses the American spelling and leaves out the optional callback.
-module(square).
-behavior(shape).
-export([name/0, area/1]).

name() -> square.

area({square, Side}) -> Side * Side.

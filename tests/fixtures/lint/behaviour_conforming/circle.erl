%% Exports every required callback and leaves out the optional one.
-module(circle).
-behaviour(shape).
-export([name/0, area/1]).

name() -> circle.

area({circle, R}) -> 3 * R * R.

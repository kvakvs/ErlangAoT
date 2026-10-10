-module(circle).
-behaviour(shape).
-export([name/0, area/1, scale/2]).

name() -> circle.

area({circle, R}) -> 3 * R * R.

scale({circle, R}, Factor) -> {circle, R * Factor}.

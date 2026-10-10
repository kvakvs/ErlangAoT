%% Informational attributes: kept in source order, a value that is not a list wrapped in one.
-module(info_target).
-vsn("2.1").
-author("Clause").
-behaviour(info_shape).
-export([area/1, scale/2]).
-copyright({2026, clause}).
-tags([alpha, beta]).
-tags(gamma).
-settings(#{name => <<"x">>}).
-empty([]).
-deprecated([{scale, 2}]).

area({square, Side}) -> Side * Side.

scale(Shape, _Factor) -> helper(Shape).

helper(Shape) -> Shape.

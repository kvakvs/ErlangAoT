-module(catch_unsafe).
-export([f/1]).
f(A) ->
    catch (X = A),
    X.

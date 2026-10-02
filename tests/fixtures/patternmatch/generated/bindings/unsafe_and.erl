-module(unsafe_and).
-export([f/1]).
f(A) -> A andalso (X = 1), X.

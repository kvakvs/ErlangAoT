-module(pat_size_bad_modifier).
-export([f/1]).
f(<<X:(bit_size(<<1/banana>>))>>) -> X.

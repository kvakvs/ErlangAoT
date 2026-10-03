-module(pat_segment_nested).
-export([f/1]).
f(<<<<X>>/binary>>) -> X.

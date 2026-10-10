%% A behaviour with two required callbacks and an optional one.
-module(shape).

-callback name() -> atom().
-callback area(term()) -> number().
-callback scale(term(), number()) -> term().
-optional_callbacks([scale/2]).

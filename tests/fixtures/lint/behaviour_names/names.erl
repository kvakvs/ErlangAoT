%% Names that cannot be modules are errors besides the undefined behaviour warnings.
-module(names).
-behaviour('').
-behaviour(' ').
-behaviour('a\tb').
-behaviour('λ').

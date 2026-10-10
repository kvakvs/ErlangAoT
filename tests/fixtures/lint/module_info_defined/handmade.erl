%% The compiler adds module_info/0,1 to every module: defining one is an error, exporting one only a warning.
-module(handmade).
-export([module_info/0, f/0]).

module_info() -> mine.

f() -> module_info(exports).

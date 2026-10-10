%% A repeated export, also of the generated module_info/1, is only a warning.
-module(repeated).
-export([a/0]).
-export([b/0, a/0]).
-export([module_info/1]).

a() -> module_info(module).

b() -> fun module_info/0.

-module(imp_lib).
-export([double/1, integer_to_list/1]).
-compile({no_auto_import, [integer_to_list/1]}).

double(X) -> 2 * X.

integer_to_list(_) -> mine.

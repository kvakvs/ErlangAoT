%% Importing a function named like an auto-imported BIF warns, unless no_auto_import names it.
-module(bifs).
-compile({no_auto_import, [hd/1]}).
-import(mylib, [length/1, hd/1, size/1]).

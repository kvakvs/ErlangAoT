%% erl_lint rejects defining an imported function and importing one function twice (that whole attribute).
-module(clash).
-export([member/2]).
-import(lists, [member/2]).
-import(lists, [reverse/1]).
-import(ordsets, [reverse/1, union/2]).

member(_, _) -> false.

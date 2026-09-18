-module(values).
-define(V, {header, $λ}).
values() -> {[], {}, [a|[]], [a|([b|Tail])], [a,b|Tail], _, _Named, ?V, (({ok})),
             16#FFFF_FFFF_FFFF_FFFF_FFFF, 1.25, $A, 65,
             "hé" "λ", ~s"hello\n", ~S"hello\n", ~"λ", ~b"x", ~B"\n",
             """
             multi
             line
             """}.

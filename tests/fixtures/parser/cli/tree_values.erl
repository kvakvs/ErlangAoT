-module(tree).
f(X) when is_integer(X), X > 0; X =:= 0 ->
  {[], [X|tail], (X), 1.25, $\n, "a\n\"Î»", 'fun',
   -X, X + 1, X = 1, catch m:f(X),
   #{a => 1}, X#{a := 2}, #r{a=1,_=X}, X#r.a, #r.a,
   #m:r{a=1}, X#_{a=1}, <<>>, <<X:8/integer-unit:8, X>>, ~b"Î»"};
f(_) -> ok.

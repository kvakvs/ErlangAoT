-module(maps_records).
-define(FIELD, a = 42).
f(#{K => f(), a := X}, #State{_ = Y}, #mod:rec{a = X}, #_{a = X}, #r.a) ->
    {#{}, #{a => 1, (K + 1) := f()}, M#{a => V}#{b := W},
     #r{}, #r{?FIELD, _ = default, X = g()}, R#r{a = 2}, R#r.a,
     #State{}, #mod:rec{}, #_{}, R#mod:rec{a = 1}, R#mod:rec.a, R#_{a = 1}, R#_.a,
     #end{}, #mod:State{}, #mod:end{}, R#State.a, R#end.a,
     #r{}#s{a = 1}#t.a, #mod:r{}#s{}, (#{})#r{}, (#r{})#{},
     (R#r{})#mod:s{}, (f())#{a => 1}, (f())#r{}, #r.a#s.b,
     M:F#r{}, A + B#{a => 1}, -R#r.a, #r{}(X)}.

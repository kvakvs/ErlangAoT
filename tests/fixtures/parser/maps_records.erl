-module(maps_records).
-record(r,{field=0}).
-record #State{field=1}.
f(X) -> #{a=>1}, X#{a:=2}, #r{field=2}, X#r.field, #r.field, #mod:rec{}, #_{field=1}, X#_.field.

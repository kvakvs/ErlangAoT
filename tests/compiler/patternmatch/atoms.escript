%% Compare term spellings and booleans through OTP, independent of atom IDs.
main([Directory]) ->
    lists:foreach(fun(Name) ->
        {ok, Module, Binary} = compile:file(filename:join(Directory, Name ++ ".erl"), [binary]),
        {module, Module} = code:load_binary(Module, Name, Binary)
    end, ["answer", "client"]),
    true = client:truth(),
    false = client:falsity(),
    lists:foreach(fun(Name) ->
        Value = apply(client, Name, []),
        io:format("~ts~n", [string:lowercase(binary:encode_hex(atom_to_binary(Value, utf8)))])
    end, [truth, falsity, ok_value, unicode, empty, nul, projected]).

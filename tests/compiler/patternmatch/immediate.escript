%% Execute complete kernels and print values/reasons independently of native tagged words.
main([Root]) ->
    lists:foreach(fun(Module) ->
        Path = filename:join(Root, atom_to_list(Module) ++ ".erl"),
        {ok, Module, Binary, _} = compile:file(Path, [binary, return_errors, return_warnings]),
        {module, Module} = code:load_binary(Module, Path, Binary)
    end, [answer, client]),
    {ok, Calls} = file:consult(filename:join(Root, "calls.term")),
    lists:foreach(fun({Module, Function, Arguments}) ->
        try apply(Module, Function, Arguments) of
            Value -> io:format("~s~n", [token(Value)])
        catch error:{badmatch,Value} -> io:format("error:badmatch:~s~n", [token(Value)]);
              error:{badarg,Value} -> io:format("error:badarg_value:~s~n", [token(Value)]);
              error:Reason -> io:format("error:~p~n", [Reason]) end
    end, Calls).

%% Serialize the admitted domain using stable spellings and mathematical integers.
token(Value) when is_integer(Value) -> "i" ++ integer_to_list(Value);
token(Value) when is_atom(Value) -> "a" ++ binary_to_list(binary:encode_hex(atom_to_binary(Value, utf8), lowercase));
token([]) -> "nil";
token({}) -> "tuple";
token(Value) when is_tuple(Value) -> "t(" ++ lists:join(",", lists:map(fun token/1, tuple_to_list(Value))) ++ ")";
token([Head|Tail]) -> "c(" ++ token(Head) ++ "," ++ token(Tail) ++ ")".

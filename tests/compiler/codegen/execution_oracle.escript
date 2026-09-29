%% Compile accepted source independently and evaluate the exact native call stream.
main([Root]) ->
    lists:foreach(fun(Name) -> load(Root, Name) end, ["answer", "client"]),
    {ok, Data} = file:read_file(filename:join(Root, "calls.txt")),
    lists:foreach(fun call/1, binary:split(Data, <<"\n">>, [global, trim_all])).

%% Keep compiler warnings out of the value protocol while preserving hard failures.
load(Root, Name) ->
    case compile:file(filename:join(Root, Name ++ ".erl"),
                      [binary, return_errors, return_warnings]) of
        {ok, Module, Binary, _Warnings} ->
            {module, Module} = code:load_binary(Module, Name ++ ".erl", Binary);
        Failure -> erlang:error({compile_failed, Name, Failure})
    end.

%% Bound generation in the Python owner; require arity agreement before applying.
call(Line) ->
    [Module, Function, Arity | Values] = string:lexemes(string:trim(binary_to_list(Line)), " "),
    Args = lists:map(fun list_to_integer/1, Values),
    true = length(Args) =:= list_to_integer(Arity),
    Result = apply(list_to_atom(Module), list_to_atom(Function), Args),
    true = is_integer(Result),
    io:format("~B~n", [Result]).

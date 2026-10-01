%% Compare pattern legality; executable matching remains a later milestone.
main([Root]) ->
    {ok, Version} = file:read_file(filename:join([code:root_dir(), "releases",
                                               erlang:system_info(otp_release), "OTP_VERSION"])),
    io:format("otp=~s erts=~s~n", [string:trim(Version), erlang:system_info(version)]),
    {ok, Cases} = file:consult(filename:join(Root, "patterns.term")),
    lists:foreach(fun(Row) -> check(Root, Row) end, Cases).

%% Warnings about impossible matches are accepted; lint errors retain independent OTP categories.
check(Root, {Name, Expected, Diagnostic}) ->
    Result = compile:file(filename:join(Root, atom_to_list(Name) ++ ".erl"),
                          [binary, return_errors, return_warnings]),
    Actual = case Result of
        {ok, _, _, _} -> accepted;
        {error, _, _} -> rejected
    end,
    Text = lists:flatten(io_lib:format("~0p", [Result])),
    case Actual =:= Expected andalso
         (Diagnostic =:= none orelse string:find(Text, atom_to_list(Diagnostic)) =/= nomatch) of
        true -> io:format("~s ~s ~s~n", [Name, Actual, Diagnostic]);
        false -> erlang:error({oracle_mismatch, Name, Expected, Diagnostic, Result})
    end.

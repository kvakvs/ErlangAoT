%% Compare compile-time binding legality without claiming executable pattern support.
main([Root]) ->
    {ok, Version} = file:read_file(filename:join([code:root_dir(), "releases",
                                               erlang:system_info(otp_release), "OTP_VERSION"])),
    io:format("otp=~s erts=~s~n", [string:trim(Version), erlang:system_info(version)]),
    {ok, Cases} = file:consult(filename:join(Root, "bindings.term")),
    lists:foreach(fun(Row) -> check(Root, Row) end, Cases),
    -42 = answer:force_succ_regs(17, -42),
    -134217728 = client:id(-134217728),
    -7 = answer:identity(-7),
    io:format("native helper oracle: -42 -134217728 -7~n").

%% Keep OTP's exact error category independent of the compiler's human diagnostic.
check(Root, {Name, Expected, Diagnostic}) ->
    Result = compile:file(filename:join(Root, atom_to_list(Name) ++ ".erl"),
                          [binary, return_errors, return_warnings]),
    Actual = case Result of
        {ok, Module, Binary, _} ->
            {module, Module} = code:load_binary(Module, atom_to_list(Name), Binary),
            accepted;
        {error, _, _} -> rejected
    end,
    Expected = Actual,
    Text = lists:flatten(io_lib:format("~0p", [Result])),
    true = Diagnostic =:= none orelse string:find(Text, atom_to_list(Diagnostic)) =/= nomatch,
    io:format("~s ~s ~s~n", [Name, Actual, Diagnostic]).

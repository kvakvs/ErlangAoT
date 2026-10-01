%% Keep syntax, semantic acceptance and observable execution as separate evidence.
main([Root, Baseline]) ->
    {ok, Version} = file:read_file(filename:join([code:root_dir(), "releases",
                                               erlang:system_info(otp_release), "OTP_VERSION"])),
    io:format("otp=~s erts=~s~n", [string:trim(Version), erlang:system_info(version)]),
    {ok, Acceptance} = file:consult(filename:join(Root, "acceptance.term")),
    lists:foreach(fun(Row) -> acceptance(Root, Row) end, Acceptance),
    lists:foreach(fun(Name) -> load(filename:join(Baseline, Name ++ ".erl")) end,
                  ["answer", "client"]),
    {ok, Cases} = file:consult(filename:join(Root, "cases.term")),
    lists:foreach(fun observe/1, Cases).

%% epp returns error forms without failing its outer result; inspect those explicitly.
acceptance(Root, {Name, Syntax, Semantic, Diagnostic}) ->
    Path = filename:join(Root, atom_to_list(Name) ++ ".erl"),
    {ok, Forms} = epp:parse_file(Path, [], []),
    Errors = [Error || {error, Error} <- Forms],
    Syntax = case Errors of [] -> accepted; _ -> rejected end,
    Result = compile:forms(Forms, [binary, return_errors, return_warnings]),
    Actual = case Result of
        {ok, Module, Binary, _Warnings} ->
            {module, Module} = code:load_binary(Module, Path, Binary),
            accepted;
        {error, _, _} -> rejected
    end,
    Semantic = Actual,
    case Diagnostic =:= none orelse
        string:find(lists:flatten(io_lib:format("~0p", [Result])), atom_to_list(Diagnostic)) =/= nomatch of
        true -> ok;
        false -> erlang:error({unexpected_diagnostic, Name, Diagnostic, Result})
    end,
    io:format("acceptance ~s syntax=~s semantic=~s diagnostic=~s~n",
              [Name, Syntax, Semantic, Diagnostic]).

%% Helpers are wrapped in new module declarations but retain their upstream clauses.
load(Path) ->
    {ok, Module, Binary, _} = compile:file(Path, [binary, return_errors, return_warnings]),
    {module, Module} = code:load_binary(Module, Path, Binary).

%% Compare stable values and error class/reason; stack traces are deliberately excluded.
observe({Label, Module, Function, Arguments, Expected}) ->
    Actual = try apply(Module, Function, Arguments) of
        Value -> {ok, Value}
    catch Class:Reason -> {error, Class, Reason}
    end,
    case Actual of
        Expected -> io:format("case ~s ~0p~n", [Label, Actual]);
        _ -> erlang:error({oracle_disagreement, Label, Expected, Actual})
    end.

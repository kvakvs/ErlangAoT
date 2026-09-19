#!/usr/bin/env escript
%% Instrument the pinned grammar's reductions; fixture seeds alone are not coverage.
-mode(compile).

main([Root, Fixtures, Work, Report]) ->
    ok = filelib:ensure_dir(filename:join(Work, "placeholder")),
    Grammar = filename:join(Root, "lib/stdlib/src/erl_parse.yrl"),
    {ok, Bytes} = file:read_file(Grammar),
    Lines = binary:split(Bytes, <<"\n">>, [global]),
    {Instrumented, _} = lists:mapfoldl(fun instrument/2, {1, none}, Lines),
    Target = filename:join(Work, "erlangaot_coverage.yrl"),
    ok = file:write_file(Target, lists:join(<<"\n">>, Instrumented)),
    {ok, Generated} = yecc:file(Target),
    {ok, erlangaot_coverage, Binary, _} = compile:file(Generated, [binary, return_errors, return_warnings]),
    {module, erlangaot_coverage} = code:load_binary(erlangaot_coverage, Generated, Binary),
    Inputs = lists:sort(filelib:fold_files(Fixtures, "\\.(erl|reject|builder-reject)$", true,
                                         fun(Path, Acc) -> [Path | Acc] end, [])),
    lists:foreach(fun(Path) -> scan(Path, Fixtures) end, Inputs),
    {ok, Inventory} = file:read_file(filename:join(Fixtures, "grammar.tsv")),
    [_Header | Rows] = string:split(binary_to_list(Inventory), "\n", all),
    Results = [row(R) || R <- Rows, R =/= ""],
    ok = file:write_file(Report, ["reference_line\tstatus\tfixtures\tproduction\n", Results]),
    Missing = [R || R <- Results, lists:member("pending", string:split(lists:flatten(R), "\t", all))],
    io:format("Grammar reductions: ~B rows, ~B ordinary rows pending; report ~s~n",
              [length(Results),length(Missing),Report]),
    case Missing of [] -> ok; _ -> halt(1) end.

instrument(Line, {N, Previous}) ->
    Pending = case re:run(Line, "^[a-z][a-z_0-9]* +->") of
                  {match,_} -> N;
                  nomatch -> Previous
              end,
    case {Pending, re:run(Line, "^.*?:(?=\\s|$)", [{capture, first, binary}])} of
        {none,_} -> {Line,{N+1,none}};
        {_,{match, [Prefix]}} ->
            <<Prefix:(byte_size(Prefix))/binary, Rest/binary>> = Line,
            {[Prefix, io_lib:format(" put({erlangaot_row,~B},true), ",[Pending]), Rest],{N+1,none}};
        {_,nomatch} -> {Line,{N+1,Pending}}
    end.

scan(Path, Fixtures) ->
    erase(),
    {ok, Epp} = epp:open([{name,Path}]),
    try forms(Epp, filename:extension(Path) =:= ".builder-reject") after epp:close(Epp) end,
    Prefix = Fixtures ++ "/",
    Relative = lists:nthtail(length(Prefix), Path),
    lists:foreach(fun({{erlangaot_row,N},true}) ->
                          Existing = persistent_term:get({?MODULE,N}, []),
                          persistent_term:put({?MODULE,N}, [Relative | Existing]);
                     (_) -> ok
                  end, get()).

forms(Epp, BuilderException) ->
    case epp:scan_erl_form(Epp) of
        {eof,_} -> ok;
        {ok,Tokens} ->
            %% Record exceptional builder inputs explicitly alongside rejection fixtures.
            try erlangaot_coverage:parse_form(Tokens)
            catch error:function_clause when BuilderException -> ok;
                  error:{badmatch,_} when BuilderException -> ok
            end,
            forms(Epp, BuilderException);
        {error,_} -> forms(Epp, BuilderException);
        {warning,_} -> forms(Epp, BuilderException)
    end.

row(Row) ->
    [Line,Step,_Family,_Seed,Production] = string:split(Row, "\t", all),
    Paths = lists:usort(persistent_term:get({?MODULE,list_to_integer(Line)}, [])),
    Status = case {Step,Paths} of {"excluded",_} -> "excluded-ssa"; {_,[]} -> "pending"; _ -> "observed" end,
    %% Retain one reproducible witness, preferring a successful fixture when available.
    Positive = [P || P <- Paths, filename:extension(P) =:= ".erl", filename:basename(P) =/= "bad.erl"],
    Witness = case {Positive,Paths} of {[P|_],_} -> P; {[],[P|_]} -> P; _ -> "-" end,
    [Line,"\t",Status,"\t",Witness,"\t",Production,"\n"].

#!/usr/bin/env escript
%% Private semantic oracle: preserve token values/order; file events are tested separately.
main([Input]) ->
    VersionFile = filename:join([code:root_dir(), "releases", "29", "OTP_VERSION"]),
    case file:read_file(VersionFile) of
        {ok, Version} ->
            case string:trim(binary_to_list(Version)) of
                "29.1" -> run(Input);
                _ -> io:format("SKIP: OTP 29.1 is required~n"), halt(77)
            end;
        _ -> io:format("SKIP: OTP 29.1 is required~n"), halt(77)
    end.
run(Input) ->
    {ok, {Features, Keywords}} = erl_features:init_parse_state([], fun erl_scan:f_reserved_word/1),
    {ok, Epp} = epp:open([{name, Input}, {location, {1,1}},
                         {features, Features}, {reserved_word_fun, Keywords}]),
    try events(Epp, filename:dirname(Input)) after try epp:close(Epp) catch exit:_ -> ok end end.
events(Epp, Root) ->
    case epp:scan_erl_form(Epp) of
        {eof, _} -> ok;
        {ok, [{'-',_},{atom,_,file}|_]} -> events(Epp, Root);
        {ok, Tokens} -> lists:foreach(fun(T) -> emit(T, Root) end, Tokens), events(Epp, Root);
        {error, Error} -> io:format("error~n"), io:format(standard_error, "~tp~n", [Error]), events(Epp, Root);
        {warning, _} -> io:format("warning~n"), events(Epp, Root)
    end.
emit({Kind, _, Value}, Root) -> emit(Kind, Value, Root);
emit({dot, _}, Root) -> emit(dot, ".", Root);
emit({Symbol, _}, Root) ->
    Kind = case erl_scan:reserved_word(Symbol) of true -> keyword; false -> symbol end,
    emit(Kind, atom_to_list(Symbol), Root).
emit(Kind, Value, Root) -> io:format("~s\t~s~n", [Kind, encoded(Value, Root)]).
encoded([], _) -> "-";
encoded(Value, _) when is_float(Value) -> binary:encode_hex(<<Value:64/float>>, lowercase);
encoded(Value, Root) when is_integer(Value) -> encoded(integer_to_list(Value), Root);
encoded(Value, Root) when is_atom(Value) -> encoded(atom_to_list(Value), Root);
encoded(Value, Root) ->
    Normalized = lists:flatten(string:replace(Value, Root, "<FIXTURES>", all)),
    binary:encode_hex(unicode:characters_to_binary(Normalized), lowercase).

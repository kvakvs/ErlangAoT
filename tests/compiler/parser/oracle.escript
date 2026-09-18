#!/usr/bin/env escript
%% Produce private normalized abstract forms, never a public stage format.
main([Mode, Input]) ->
    VersionFile = filename:join([code:root_dir(), "releases", "29", "OTP_VERSION"]),
    case file:read_file(VersionFile) of
        {ok, Version} ->
            case string:trim(binary_to_list(Version)) of
                "29.1" -> run(Mode, Input);
                _ -> skip()
            end;
        _ -> skip()
    end.

skip() -> io:format("SKIP: OTP 29.1 is required~n"), halt(77).

run("raw", Input) ->
    {ok, File} = file:open(Input, [read, {encoding, utf8}]),
    try raw(File, {1,1}) after file:close(File) end;
run("epp", Input) -> with_epp(Input, fun expanded/1);
run("phase1", Input) -> with_epp(Input, fun phase1/1);
run("lint", Input) ->
    {ok, Forms} = epp:parse_file(Input, [], []),
    Result = case erl_lint:module(Forms, Input) of
        {ok, _} -> ok;
        {error, _, _} -> error
    end,
    emit({lint, Result}).

raw(File, Location) ->
    case io:scan_erl_form(File, '', Location) of
        {ok, Tokens, Next} -> result(erl_parse:parse_form(Tokens)), raw(File, Next);
        {error, Error, Next} -> result({error, Error}), raw(File, Next);
        {eof, _} -> ok
    end.

expanded(Epp) ->
    case epp:parse_erl_form(Epp) of
        {eof, _} -> ok;
        Event -> result(Event), expanded(Epp)
    end.

result({ok, Form0}) ->
    Form = erl_parse:map_anno(fun(_) -> 0 end, Form0),
    emit({ok, normalize_file(Form)});
result({error, {Location, Module, _Description}}) -> emit({error, Module, Location});
result({warning, {Location, Module, _Description}}) -> emit({warning, Module, Location}).

normalize_file({attribute, A, file, {Path, Line}}) ->
    {attribute, A, file, {filename:basename(Path), Line}};
normalize_file(Form) -> Form.

%% Preserve binary64 bits and map keys; annotations alone are normalized away.
stable(Value) when is_float(Value) -> {float_bits, binary:encode_hex(<<Value:64/float>>, lowercase)};
stable(Value) when is_tuple(Value) -> list_to_tuple([stable(X) || X <- tuple_to_list(Value)]);
stable([H|T]) -> [stable(H)|stable(T)];
stable(Value) when is_map(Value) -> {map_value, lists:sort([{stable(K), stable(V)} || K := V <- Value])};
stable(Value) -> Value.

emit(Value) -> io:format("~0tp.~n", [stable(Value)]).

%% Keep the Phase I projection deliberately closed: unknown forms fail the oracle.
with_epp(Input, Consumer) ->
    {ok, Epp} = epp:open([{name, Input}, {location, {1,1}}]),
    try Consumer(Epp) after epp:close(Epp) end.

phase1(Epp) ->
    case epp:parse_erl_form(Epp) of
        {eof, _} -> ok;
        {ok, Form} -> project(Form), phase1(Epp);
        Other -> erlang:error({unexpected_phase1_event, Other})
    end.

project({attribute, _, module, Name}) -> field("module", atom_to_list(Name));
project({attribute, _, file, {Name, Line}}) ->
    io:format("file\t~s\t~B~n", [hex(filename:basename(Name)), Line]);
project({function, _, Name, 0, [{clause, _, [], [], [Expr]}]}) ->
    field("function", atom_to_list(Name)), scalar(Expr);
project(Other) -> erlang:error({unmapped_phase1_form, Other}).

scalar({atom, _, Name}) -> field("atom", atom_to_list(Name));
scalar({integer, _, Value}) -> io:format("integer\t~B~n", [Value]);
scalar({float, _, Value}) -> io:format("float\t~s~n", [binary:encode_hex(<<Value:64/float>>, lowercase)]);
scalar({char, _, Value}) -> io:format("char\t~B~n", [Value]);
scalar({string, _, Value}) -> field("string", Value);
scalar(Other) -> erlang:error({unmapped_phase1_expression, Other}).

field(Kind, Value) -> io:format("~s\t~s~n", [Kind, hex(Value)]).
hex([]) -> "-";
hex(Value) -> binary:encode_hex(unicode:characters_to_binary(Value), lowercase).

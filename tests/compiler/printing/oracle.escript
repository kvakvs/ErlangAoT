#!/usr/bin/env escript
%% Observes OTP term printing for owned wire values (tests/compiler/codegen/match_wire.hpp grammar).
%% values: one "<hex ~w UTF-8>\t<hex emulator text>" line per input value. ~w text uses the
%% documented map-key order (maps_order ordered, as ~kw); the emulator text keeps OTP's internal
%% map order, which regenerate.py only trusts where it cannot depend on atom indices or hashing.
%% calls: compiles answer/client and runs each call, printing through the real erlang:display/1.
-mode(compile).

main([VersionFile | Mode]) ->
    ok = file:write_file(VersionFile, io_lib:format("~s~n~s~n", [release(), otp_version()])),
    run(Mode).

run(["values", Input, Output]) ->
    Lines = lines(Input),
    ok = file:write_file(Output, [observe(decode(Line)) || Line <- Lines]);
run(["calls", SourceDir, Calls]) ->
    [load(filename:join(SourceDir, File)) || File <- ["answer.erl", "client.erl"]],
    [call(string:split(Line, " ", all)) || Line <- lines(Calls)],
    ok.

%% Both renderings of one term as hex so arbitrary bytes survive the transport.
observe(Term) ->
    Options = [{depth, -1}, {encoding, latin1}, {maps_order, ordered}],
    Write = unicode:characters_to_binary(io_lib:write(Term, Options)),
    Display = list_to_binary(erts_internal:term_to_string(Term, undefined)),
    [binary:encode_hex(Write, lowercase), $\t, binary:encode_hex(Display, lowercase), $\n].

%% "module function arity args..." runs once; the true result is written on the display channel.
call([Module, Function, _Arity | Args]) ->
    Result = apply(binary_to_atom(Module), binary_to_atom(Function), [decode(Arg) || Arg <- Args]),
    true = Result,
    erlang:display_string(stdout, "a74727565\n").

%% Compiles one module in memory, failing on any warning.
load(File) ->
    {ok, Module, Binary, []} = compile:file(File, [
        binary, return_errors, return_warnings, warnings_as_errors
    ]),
    {module, Module} = code:load_binary(Module, File, Binary).

lines(File) ->
    {ok, Text} = file:read_file(File),
    [Line || Line <- binary:split(Text, [<<"\n">>, <<"\r\n">>], [global]), Line =/= <<>>].

decode(Text) ->
    {Term, <<>>} = term(Text),
    Term.

term(<<"t(", Rest/binary>>) ->
    {Items, Tail} = items(Rest, []),
    {list_to_tuple(Items), Tail};
term(<<"c(", Rest/binary>>) ->
    {[Head, Tail0], Tail} = items(Rest, []),
    {[Head | Tail0], Tail};
term(<<"m(", Rest/binary>>) ->
    {Items, Tail} = items(Rest, []),
    {maps:from_list([{K, V} || {K, V} <- Items]), Tail};
term(Text) ->
    [Token | _] = binary:split(Text, [<<",">>, <<")">>]),
    {scalar(Token), binary:part(Text, byte_size(Token), byte_size(Text) - byte_size(Token))}.

items(<<")", Rest/binary>>, Acc) ->
    {lists:reverse(Acc), Rest};
items(Text, Acc) ->
    case term(Text) of
        {Item, <<",", Rest/binary>>} -> items(Rest, [Item | Acc]);
        {Item, <<")", Rest/binary>>} -> {lists:reverse([Item | Acc]), Rest}
    end.

scalar(<<"nil">>) ->
    [];
scalar(<<"tuple">>) ->
    {};
scalar(<<"a", Hex/binary>>) ->
    binary_to_atom(binary:decode_hex(Hex), utf8);
scalar(<<"i", Decimal/binary>>) ->
    binary_to_integer(Decimal);
scalar(<<"f", Hex/binary>>) ->
    <<Float/float>> = binary:decode_hex(Hex),
    Float;
scalar(<<"b", Bits/binary>>) ->
    [Count, Hex] = binary:split(Bits, <<":">>),
    Size = binary_to_integer(Count),
    <<Value:Size/bitstring, _/bitstring>> = binary:decode_hex(Hex),
    Value.

release() -> erlang:system_info(otp_release).

otp_version() ->
    Path = filename:join([code:root_dir(), "releases", release(), "OTP_VERSION"]),
    {ok, Version} = file:read_file(Path),
    string:trim(Version).

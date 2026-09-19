#!/usr/bin/env escript
%% Compare the pinned scanner's categories, locations, and decoded values.
main([Input]) ->
    case list_to_integer(erlang:system_info(otp_release)) >= 29 of
        true -> ok;
        false -> io:format(standard_error, "OTP 29 or newer is required~n", []), halt(1)
    end,
    {ok, Bytes} = file:read_file(Input),
    Encoding = case epp:read_encoding(Input) of none -> utf8; E -> E end,
    Chars = unicode:characters_to_list(Bytes, Encoding),
    {ok, Tokens, _} = erl_scan:string(Chars, {1,1}, [return_comments]),
    lists:foreach(fun emit/1, Tokens).

emit({Kind, Anno, Value}) -> emit(Kind, Anno, Value);
emit({dot, Anno}) -> emit(dot, Anno, ".");
emit({Symbol, Anno}) ->
    Kind = case erl_scan:reserved_word(Symbol) of true -> keyword; false -> symbol end,
    emit(Kind, Anno, atom_to_list(Symbol)).

emit(Kind, Anno, Value) ->
    {Line, Column} = erl_anno:location(Anno),
    io:format("~s\t~B\t~B\t~s~n", [Kind, Line, Column, encoded(Value)]).

encoded([]) -> "-";
encoded(Value) when is_float(Value) -> binary:encode_hex(<<Value:64/float>>, lowercase);
encoded(Value) when is_integer(Value) -> encoded(integer_to_list(Value));
encoded(Value) when is_atom(Value) -> encoded(atom_to_list(Value));
encoded(Value) -> binary:encode_hex(unicode:characters_to_binary(Value), lowercase).

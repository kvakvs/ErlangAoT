%% Length-prefixed frame codec. A stream starts with a one-byte header
%% (3-bit version, 1-bit flag, 4 reserved bits); each frame is
%% <<Type:8, Length:16, Payload:Length/binary, Checksum:8>>.
-module(codec).
-export([encode/1, decode/1, checksum/1]).

-define(VERSION, 2).
-define(TEXT, 1).
-define(INTS, 2).
-define(PAIR, 3).

%% Encodes messages into one stream binary with a version header.
encode(Messages) ->
    Flag = length(Messages) rem 2,
    iolist_to_binary([<<?VERSION:3, Flag:1, 0:4>> | [frame(M) || M <- Messages]]).

%% Encodes one message as a typed, checksummed frame.
frame({text, Text}) when is_binary(Text) ->
    wrap(?TEXT, Text);
frame({ints, Ints}) when is_list(Ints) ->
    wrap(?INTS, <<<<I:32/signed>> || I <- Ints>>);
frame({pair, Key, Value}) when is_atom(Key), Value >= 0, Value < 65536 ->
    Name = list_to_binary(atom_to_list(Key)),
    wrap(?PAIR, <<(byte_size(Name)):8, Name/binary, Value:16/little>>).

wrap(Type, Payload) ->
    <<Type:8, (byte_size(Payload)):16, Payload/binary, (checksum(Payload)):8>>.

%% Rotating XOR checksum over all payload bytes.
checksum(Bin) -> checksum(Bin, 0).

checksum(<<Byte, Rest/binary>>, Acc) ->
    Rotated = ((Acc bsl 1) bor (Acc bsr 7)) band 16#FF,
    checksum(Rest, Rotated bxor Byte);
checksum(<<>>, Acc) ->
    Acc.

%% Decodes a stream into {ok, Header, Messages} or {error, Reason}.
decode(<<Version:3, Flag:1, _:4, Frames/binary>>) ->
    try frames(Frames, []) of
        Messages -> {ok, {Version, Flag}, Messages}
    catch
        throw:Reason -> {error, Reason}
    end;
decode(<<>>) ->
    {error, empty}.

frames(<<>>, Acc) ->
    lists:reverse(Acc);
frames(<<Type:8, Length:16, Payload:Length/binary, Sum:8, Rest/binary>>, Acc) ->
    case checksum(Payload) of
        Sum -> frames(Rest, [payload(Type, Payload) | Acc]);
        Other -> throw({checksum, length(Acc), Sum, Other})
    end;
frames(Truncated, Acc) ->
    throw({truncated, length(Acc), byte_size(Truncated)}).

payload(?TEXT, Text) ->
    {text, Text, utf8_length(Text)};
payload(?INTS, Ints) when byte_size(Ints) rem 4 =:= 0 ->
    {ints, [I || <<I:32/signed>> <= Ints]};
payload(?PAIR, <<Size:8, Name:Size/binary, Value:16/little>>) ->
    {pair, list_to_atom(binary_to_list(Name)), Value};
payload(Type, Payload) ->
    throw({bad_payload, Type, byte_size(Payload)}).

%% Counts UTF-8 code points, rejecting malformed sequences.
utf8_length(<<>>) -> 0;
utf8_length(<<_/utf8, Rest/binary>>) -> 1 + utf8_length(Rest);
utf8_length(_) -> throw(bad_utf8).

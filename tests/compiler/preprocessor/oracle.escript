#!/usr/bin/env escript
%% Record private test events only; this is not a compiler interchange format.
main([Input, Output]) ->
    case list_to_integer(erlang:system_info(otp_release)) >= 29 of
        true -> run(Input, Output);
        false -> io:format(standard_error, "OTP 29 or newer is required~n", []), halt(1)
    end.

run(Input, Output) ->
    {ok, Epp} = epp:open([{name, Input}, {location, {1,1}}]),
    {ok, File} = file:open(Output, [write, {encoding, utf8}]),
    try record(Epp, File) after epp:close(Epp), file:close(File) end.

record(Epp, File) ->
    Event = epp:scan_erl_form(Epp),
    io:format(File, "~tp.~n", [Event]),
    case Event of {eof, _} -> ok; _ -> record(Epp, File) end.

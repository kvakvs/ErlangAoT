#!/usr/bin/env escript
%% Record private test events only; this is not a compiler interchange format.
main([Input, Output]) ->
    VersionFile = filename:join([code:root_dir(), "releases", "29", "OTP_VERSION"]),
    case file:read_file(VersionFile) of
        {ok, Version} -> check_version(string:trim(binary_to_list(Version)), Input, Output);
        _ -> io:format("SKIP: OTP 29.1 is required~n"), halt(77)
    end.

check_version("29.1", Input, Output) ->
    {ok, Epp} = epp:open([{name, Input}, {location, {1,1}}]),
    {ok, File} = file:open(Output, [write, {encoding, utf8}]),
    try record(Epp, File) after epp:close(Epp), file:close(File) end;
check_version(_, _, _) ->
    io:format("SKIP: OTP 29.1 is required~n"), halt(77).

record(Epp, File) ->
    Event = epp:scan_erl_form(Epp),
    io:format(File, "~tp.~n", [Event]),
    case Event of {eof, _} -> ok; _ -> record(Epp, File) end.

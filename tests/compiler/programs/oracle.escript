#!/usr/bin/env escript
%% Runs one program fixture under OTP with the proposed executable contract:
%% Entry:main(Args) gets argv strings, normal return exits 0, erlang:halt/1
%% sets the status and an uncaught exception reports to stderr and exits 1.
-mode(compile).

main([VersionFile, SourceDir, Entry | Args]) ->
    ok = logger:remove_handler(default),
    ok = logger:add_handler(default, logger_std_h, #{config => #{type => standard_error}}),
    ok = file:write_file(VersionFile, io_lib:format("~s~n~s~n", [release(), otp_version()])),
    [load(File) || File <- lists:sort(filelib:wildcard(filename:join(SourceDir, "**/*.erl")))],
    run(list_to_atom(Entry), Args).

%% Compiles one module in memory, failing on any warning.
load(File) ->
    case compile:file(File, [binary, return_errors, return_warnings, warnings_as_errors]) of
        {ok, Module, Binary, []} ->
            {module, Module} = code:load_binary(Module, File, Binary);
        Failure ->
            io:format(standard_error, "fixture compile failed: ~p~n", [Failure]),
            halt(125)
    end.

%% Runs the entry in a fresh process so the escript process stays out of its links.
run(Module, Args) ->
    {Pid, Ref} = spawn_monitor(fun() -> invoke(Module, Args) end),
    receive
        {'DOWN', Ref, process, Pid, normal} -> halt(0);
        {'DOWN', Ref, process, Pid, Reason} -> uncaught(exit, Reason)
    end.

invoke(Module, Args) ->
    try
        Module:main(Args)
    catch
        Class:Reason -> uncaught(Class, Reason)
    end.

uncaught(Class, Reason) ->
    io:format(standard_error, "uncaught ~w: ~p~n", [Class, Reason]),
    halt(1).

release() -> erlang:system_info(otp_release).

otp_version() ->
    Path = filename:join([code:root_dir(), "releases", release(), "OTP_VERSION"]),
    {ok, Version} = file:read_file(Path),
    string:trim(Version).

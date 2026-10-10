#!/usr/bin/env escript
%% Compiles one lint case under OTP, in its listed order and loading each
%% compiled module so later files see their behaviours, and writes the
%% diagnostics as JSON: {version, exit_status, diagnostics}.
-mode(compile).

main([Output | Files]) ->
    Diagnostics = lists:append([compile_file(File) || File <- Files]),
    Errors = [D || #{severity := ~"error"} = D <- Diagnostics],
    Status =
        case Errors of
            [] -> 0;
            _ -> 1
        end,
    Result = #{
        version => [list_to_binary(release()), otp_version()],
        exit_status => Status,
        diagnostics => Diagnostics
    },
    ok = file:write_file(Output, json:encode(Result)).

compile_file(File) ->
    case compile:file(File, [binary, return_errors, return_warnings]) of
        {ok, Module, Binary, Warnings} ->
            {module, Module} = code:load_binary(Module, File, Binary),
            messages(~"warning", Warnings);
        {error, Errors, Warnings} ->
            messages(~"error", Errors) ++ messages(~"warning", Warnings)
    end.

messages(Severity, PerFile) ->
    [
        message(Severity, File, Location, Module, Description)
     || {File, Items} <- PerFile, {Location, Module, Description} <- Items
    ].

message(Severity, File, {Line, Column}, Module, Description) ->
    Text = unicode:characters_to_binary(Module:format_error(Description)),
    #{
        file => unicode:characters_to_binary(filename:basename(File)),
        line => Line,
        column => Column,
        severity => Severity,
        message => Text
    }.

release() -> erlang:system_info(otp_release).

otp_version() ->
    Path = filename:join([code:root_dir(), "releases", release(), "OTP_VERSION"]),
    {ok, Version} = file:read_file(Path),
    string:trim(Version).

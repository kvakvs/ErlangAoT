#!/usr/bin/env escript
%% Query the runtime used by the selected escript, including version-manager shims.
main([]) ->
    Release = erlang:system_info(otp_release),
    VersionFile = filename:join([code:root_dir(), "releases", Release, "OTP_VERSION"]),
    Version = case file:read_file(VersionFile) of
        {ok, Bytes} -> string:trim(binary_to_list(Bytes));
        _ -> Release
    end,
    io:format("~s~n~s", [Release, Version]).

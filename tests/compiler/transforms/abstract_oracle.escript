#!/usr/bin/env escript
%% Regenerate the abstract format goldens with the host OTP (explicit run only), from the fixture directory:
%%   cd tests/fixtures/transforms/abstract && escript ../../../compiler/transforms/abstract_oracle.escript *.erl
%% Each Name.erl gets Name.abstr: the forms epp gives a parse transform under erlc, one `~tp.` term per form.
-mode(compile).

main(Files) ->
    lists:foreach(fun write/1, Files).

write(File) ->
    {ok, Forms} = epp:parse_file(File, [{location, {1, 1}}, {includes, ["."]}]),
    [error({File, Form}) || {error, _} = Form <- Forms],
    Text = [io_lib:format("~tp.~n", [Form]) || Form <- Forms],
    Out = filename:rootname(File) ++ ".abstr",
    ok = file:write_file(Out, unicode:characters_to_binary(Text)),
    io:format("~s: ~p forms~n", [Out, length(Forms)]).

-module(otp_headers).
-include("headers/otp_assert.hrl").
-include("headers/otp_file.hrl").
check(X) ->
    ?assert(X =:= X),
    ?assertEqual(1, X),
    ?assertMatch({ok,_}, {ok,X}),
    ?assertException(error, badarg, erlang:error(badarg)),
    #file_info{size = 0}.

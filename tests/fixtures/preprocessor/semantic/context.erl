-module('context module').
-define(LINE_NOW, ?LINE).
-define(HEADER(Name), Name(A,B)).
-define(BODY, {?MODULE, ?MODULE_STRING, ?FILE, ?LINE_NOW, ?FUNCTION_NAME, ?FUNCTION_ARITY, ?OTP_RELEASE, ?MACHINE}).
?HEADER(first) -> ?BODY;
first(_,_) -> ?BODY.
-file("logical.erl",100).
logical() -> {?FILE,?LINE}.

-extends('base module').
base() -> {?BASE_MODULE, ?BASE_MODULE_STRING, ?'LINE'}.
guarded(A) when is_atom(A) -> fun () -> {?FUNCTION_NAME, ?FUNCTION_ARITY} end.
-undef(OTP_RELEASE).
-define(OTP_RELEASE,99).
replaced() -> ?OTP_RELEASE.
-undef(LINE).
special() -> ?LINE.

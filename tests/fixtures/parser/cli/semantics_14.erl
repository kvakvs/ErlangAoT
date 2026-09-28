-module('quoted module').
-define(L, ?LINE).
f(A,B) -> {?MODULE,?MODULE_STRING,?FUNCTION_NAME,?FUNCTION_ARITY,?L,?OTP_RELEASE,?MACHINE}.
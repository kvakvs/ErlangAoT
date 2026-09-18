-module(literals).
f() -> {ok, 123456789012345678901234567890, 1.25, $a, "a" "b", ~s"text", ~"bytes", [1,2|tail]}.

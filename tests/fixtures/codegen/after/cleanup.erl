-module(cleanup).
-export([run/2]).

%% Mode 0 returns and mode 1 throws; the after body builds a Size-byte binary, which a small heap budget rejects.
run(Mode, Size) ->
    try
        body(Mode)
    after
        _ = <<0:(Size * 8)>>
    end.

body(0) -> {kept, [1, 2, 3]};
body(1) -> throw({thrown, [4, 5]}).

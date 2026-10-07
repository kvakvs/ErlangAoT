-module(shapes).
-export([circle/1, secret/0, mixed/0]).
-export_record([circle, request]).

%% Exported records may be built, read, updated and matched by other modules.
-record(#circle{radius = 1, center = {0, 0}, tags = [round, <<"c">>], meta = #{kind => shape}}).
-record #request{need, retries = 3}.
%% A private record: other modules may only test its identity.
-record(#secret{code = 42}).

circle(R) -> #circle{radius = R}.
secret() -> #secret{}.
mixed() -> {#circle{}, #secret{}}.

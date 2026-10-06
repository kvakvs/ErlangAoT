-module(geometry).
-export([origin/0, moved/2]).

-record(point, {x = 0, y = 0, z = 0}).

origin() -> #point{}.

%% Updates a record received from another module, which declares the same record.
moved(Point, Step) -> Point#point{x = Point#point.x + Step, y = Step}.

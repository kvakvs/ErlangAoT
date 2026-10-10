%% A behaviour: implementers export name/0 and area/1; scale/2 is optional.
-module(shape).
-export([describe/2]).

-callback name() -> atom().
-callback area(term()) -> number().
-callback scale(term(), number()) -> term().
-optional_callbacks([scale/2]).

%% Call the callbacks of Module; an implementer without scale/2 raises undef.
describe(Module, Shape) ->
    Scaled =
        try Module:scale(Shape, 2) of
            Bigger -> Module:area(Bigger)
        catch
            error:undef -> none
        end,
    {Module:name(), Module:area(Shape), Scaled}.

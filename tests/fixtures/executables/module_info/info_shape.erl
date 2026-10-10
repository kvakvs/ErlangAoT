%% A behaviour, so its exports end with the generated behaviour_info/1 before module_info/0,1.
-module(info_shape).

-callback area(term()) -> number().

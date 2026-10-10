%% Neither a missing module, a module without callbacks nor a library module without them is a behaviour.
-module(client).
-behaviour(no_such_behaviour).
-behaviour(plain).
-behaviour(lists).

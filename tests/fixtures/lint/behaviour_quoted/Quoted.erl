%% Callback and behaviour names that need quotes.
-module('Quoted').

-callback 'Init'(term()) -> ok.
-callback 'with space'() -> ok.
-callback plain() -> ok.

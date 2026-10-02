-module(service_client).
-export([nested/1]).
nested(X) -> service_answer:body(X).

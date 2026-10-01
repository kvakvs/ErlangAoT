-module(failure_client).
-export([run/0]).
run() -> failure_answer:take(failure_answer:chain(), failure_answer:later()).

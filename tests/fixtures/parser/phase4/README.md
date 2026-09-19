# Phase IV parser fixtures

Authored from the pinned OTP 29.1 grammar; no upstream fixture code was copied.
New `.erl.ast`/`.erl.lint` records are generated with the shared oracle adapter on
the installed Homebrew OTP 29.0.5. Native golden comparisons run offline; live
comparisons use CMake's selected OTP >=29. The grammar target remains OTP 29.1.

Step 10 covers begin, case, guard-only if, all three receive alternatives, nested
delimiters, macro arguments, permissive branch candidates, and rejected empty or
incomplete blocks. `control.erl.lint` records parse success with deferred lint errors.

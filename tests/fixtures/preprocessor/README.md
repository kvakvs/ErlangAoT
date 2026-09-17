# Preprocessor reference fixtures

Baseline: OTP-29.1, commit `751f87b703fe5948607d08e82599ce644b772e76`.
The `.tokens` files were recorded by `scan.escript` using a locally built OTP-29.1.
They contain category, line, column, and hexadecimal decoded value separated by
tabs; `-` denotes an empty value and floats use IEEE-754 binary64 bits.
This format is private to tests.

`lexer_golden` checks these expectations without Erlang. `scanner_oracle` repeats
the comparison with the pinned release when available. `good.erl`/`bad.erl` test
the raw epp event recorder. Files under `lexical/` are scanner inputs and need not
be valid parsed Erlang programs. `latin1.erl` intentionally contains Latin-1 bytes
and CRLF; keep it byte-for-byte when editing.

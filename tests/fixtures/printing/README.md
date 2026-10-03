# Term printing goldens

OTP 29 renderings of owned values, used OTP-free by `runtime_printing`
(`tests/runtime/printing.cpp`) and `printing_display`
(`tests/compiler/printing/display.py`). Rules: [printing](../../../docs/terms.md#printing).

| File | Content |
| --- | --- |
| `values.txt` | Inputs in the wire grammar of `tests/compiler/codegen/match_wire.hpp`: authored edge values (`tests/compiler/printing/values.py`), then every distinct result and error payload of the patternmatch corpora |
| `write.txt` | `io_lib` `~w` text per value, UTF-8, with maps in map-key order (`maps_order => ordered`, as `~kw`) |
| `display.txt` | `erlang:display/1` text per value; printable ASCII kept, `\\` and other bytes as `\xHH`. `?unordered` marks values whose OTP map order is internal (atom-bearing keys of multi-key maps, or more than 32 keys), which are not compared |
| `display/` | `answer.erl`/`client.erl` calling `erlang:display/1`, native calls and OTP stdout from the real `erlang:display/1` |
| `manifest.json` | Oracle version, OTP pin, generator and file hashes, counts |

Regeneration is an explicit maintainer action from the repository root:

```powershell
python tests/compiler/printing/regenerate.py --escript 'C:/Program Files/Erlang OTP/erts-17.1/bin/escript.exe'
```

`--check` regenerates into `build/` and reports drift without writing. CTest
fails when files, authored values or corpora no longer match the manifest.

"""Compare guard grouping and term-valued boolean execution with OTP through real CLI objects."""
import json
import pathlib
import sys
from stored import load
from evidence import run
from immediate import native

VALUES = ["true", "false", "glurf", 0, 7, "nil", "tuple", "Ω"]
OPERATORS = [("lazy_and", "andalso"), ("lazy_or", "orelse"), ("strict_and", "and"),
             ("strict_or", "or"), ("strict_xor", "xor")]


def inspection(tool, work):
    """Check word joins and lazy RHS blocks in verified IR; label both-width objects as inspection only."""
    source = work / "answer.erl"
    for mode in ["--print-ir", "--print-optimized-ir", "--print-types"]:
        options = [] if mode == "--print-types" else ["-O2"]
        result = run([tool, *options, mode, str(source), str(work / "client.erl")])
        if mode == "--print-ir":
            assert "lazy.right" in result and "lazy.join" in result and "phi i64" in result, result
            assert "guard.alternative" in result and "guard.next" in result, result
    widths = []
    (work / "width.erl").write_bytes(b"-module(width). -export([f/2,g/1]).\nf(X,Y) -> X andalso Y. g(X) -> not X.\n")
    for bits, triple in [(32, "i686-pc-windows-msvc"), (64, "x86_64-pc-windows-msvc")]:
        ir = run([tool, "--target-triple", triple, "--print-ir", str(work / "width.erl")])
        assert f"phi i{bits}" in ir and "erlang_aot_immediate_v1" in ir, ir
        run([tool, "--target-triple", triple, "--emit", "obj", "--artifact-dir", str(work / f"width{bits}"),
             str(work / "width.erl")])
        widths.append({"bits": bits, "triple": triple, "objects_and_verified_ir": "passed", "execution": "not attempted"})
    return widths


def main():
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, "booleans", work)
    native(tool, cmake, source, work, settings, config, suffix)
    records["widths"] = inspection(tool, work)
    (work / "evidence.json").write_text(json.dumps(records, indent=2) + "\n", encoding="utf-8")
    print(f'{records["calls"]} OTP/native guard/boolean calls; four policies, grouping, word joins, errors and recovery passed.')


if __name__ == "__main__":
    main()

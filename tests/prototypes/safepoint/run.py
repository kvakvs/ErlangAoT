"""Compile the step-24 safepoint prototype for both word widths and check that the reload survives optimization.

Usage: python run.py [--clang PATH] [--out DIR]. Not part of CTest; results are recorded in
docs/runtime-heap.md#collection-in-generated-code. Compile-only: no runtime or linker is needed.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
LEVELS = ["-O0", "-O2"]
# One target per word width and architecture family that the project supports.
TARGETS = [
    ("x86_64-pc-windows-msvc", "i64"),
    ("i686-pc-windows-msvc", "i32"),
    ("aarch64-unknown-linux-gnu", "i64"),
    ("armv7-unknown-linux-gnueabihf", "i32"),
]
# Header plus slot index of Y: the reload reads this word of the frame.
Y_WORD = 4 + 3


def find_clang(explicit):
    """Use --clang, else clang on PATH, else the default Windows LLVM install."""
    candidates = [explicit, shutil.which("clang"), os.path.join(os.environ.get("ProgramFiles", ""), "LLVM", "bin",
                                                                "clang.exe")]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    sys.exit("clang not found; pass --clang")


def run(command):
    """Run a command and return (exit status, combined output)."""
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.returncode, result.stdout + result.stderr


def reloads_after_safepoint(ir, word):
    """Whether a load of Y's frame word follows the safepoint call (O0 keeps the named GEP, O2 a byte offset)."""
    after = ir.split("call void @CLAUSE_safepoint_v1", 1)
    if len(after) != 2:
        return False
    offset = Y_WORD * (8 if word == "i64" else 4)
    names = re.findall(rf"(%[\w.]+) = getelementptr inbounds (?:nuw )?(?:i8, ptr %\w+, i(?:32|64) {offset}|{word}, ptr "
                       rf"%slots, i32 3)", ir)
    return any(re.search(rf"load {word}, ptr {re.escape(name)}\b", after[1]) for name in names)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang")
    parser.add_argument("--out")
    args = parser.parse_args()
    clang = find_clang(args.clang)
    out = Path(args.out) if args.out else Path(tempfile.mkdtemp(prefix="safepoint-"))
    out.mkdir(parents=True, exist_ok=True)
    print(f"clang: {run([clang, '--version'])[1].splitlines()[0]}\nout: {out}\n")
    source = (HERE / "loop.ll").read_text(encoding="utf-8")
    passed = True
    for target, word in TARGETS:
        typed = out / f"loop-{word}.ll"
        typed.write_bytes(source.replace("WORD", word).encode("utf-8"))
        for level in LEVELS:
            base = [clang, f"--target={target}", level, "-Wno-override-module", str(typed)]
            ir_path, asm_path = out / f"{target}{level}.ll", out / f"{target}{level}.s"
            status, text = run([*base, "-S", "-emit-llvm", "-o", str(ir_path)])
            if status == 0:
                status, text = run([*base, "-S", "-o", str(asm_path)])
            ok = status == 0 and reloads_after_safepoint(ir_path.read_text(encoding="utf-8"), word)
            print(f"{target:32} {level} word={word} {'reload ok' if ok else 'FAIL'}{'' if status == 0 else text}")
            passed = passed and ok
    print("PASS" if passed else "FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

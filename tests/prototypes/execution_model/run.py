"""Build and run the step-17 execution-model prototypes and inspect the chosen lowering on every target.

Usage: python run.py [--clang PATH] [--out DIR]. Not part of CTest; results are recorded in
docs/execution-model.md. Host runs need a clang++ that can link C++ programs; target checks are compile-only.
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
FLAGS = ["-std=c++23", "-Wall", "-Wextra", "-Werror"]
LEVELS = ["-O0", "-O2"]
# Host programs: label, sources and extra defines.
PROGRAMS = [
    ("frames-musttail", ["generated.cpp", "runtime.cpp"], []),
    ("frames-trampoline", ["generated.cpp", "runtime.cpp"], ["-DPROTOTYPE_TRAMPOLINE"]),
    ("native-calls", ["native.cpp"], []),
    ("llvm-coroutines", ["coroutines.cpp"], []),
]
# Required targets (AGENTS.md): Windows x86, Linux x86/ARM, macOS Apple Silicon, both word widths.
TARGETS = [
    "x86_64-pc-windows-msvc",
    "i686-pc-windows-msvc",
    "x86_64-unknown-linux-gnu",
    "i686-unknown-linux-gnu",
    "aarch64-unknown-linux-gnu",
    "armv7-unknown-linux-gnueabihf",
    "arm64-apple-macosx14.0",
]


def find_clang(explicit):
    """Use --clang, else clang++ on PATH, else the default Windows LLVM install."""
    candidates = [explicit, shutil.which("clang++"), os.path.join(os.environ.get("ProgramFiles", ""), "LLVM",
                                                                  "bin", "clang++.exe")]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    sys.exit("clang++ not found; pass --clang")


def run(command):
    """Run a command and return (exit status, combined output)."""
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    return result.returncode, result.stdout + result.stderr


def host_runs(clang, out):
    """Build every prototype at O0 and O2 for the host and run it; return True when all pass."""
    passed = True
    for label, sources, defines in PROGRAMS:
        for level in LEVELS:
            binary = out / f"{label}{level}.exe"
            status, text = run([clang, *FLAGS, level, *defines, *[str(HERE / s) for s in sources], "-o",
                                str(binary)])
            if status == 0:
                status, text = run([str(binary)])
            print(f"### {label} {level} exit={status}\n{text.rstrip()}\n")
            passed = passed and status == 0
    return passed


def transfer_sites():
    """Number of TRANSFER uses in the hand-lowered functions."""
    return (HERE / "generated.cpp").read_text(encoding="utf-8").count("TRANSFER(")


def tail_jumps(assembly):
    """Count tail jumps: x86 marks them TAILCALL; ARM branches (b/br/bx) to a function or register, not lr."""
    arm = re.findall(r"^\s+(?:b|br|bx)\s+(?:[xr]\d+|_+ZN9prototype\w+)\s*$", assembly, re.MULTILINE)
    return assembly.count("TAILCALL") + len(arm)


def target_checks(clang, out):
    """Compile the generated functions for each target; count musttail calls in IR and tail jumps in assembly."""
    expected, passed = transfer_sites(), True
    print(f"### targets (generated.cpp, {expected} transfer sites)")
    for target in TARGETS:
        for level in LEVELS:
            base = [clang, f"--target={target}", "-ffreestanding", *FLAGS, level, str(HERE / "generated.cpp")]
            ir_path, asm_path = out / f"{target}{level}.ll", out / f"{target}{level}.s"
            status, text = run([*base, "-S", "-emit-llvm", "-o", str(ir_path)])
            if status == 0:
                status, text = run([*base, "-S", "-o", str(asm_path)])
            if status != 0:
                print(f"{target:32} {level} FAIL\n{text}")
                passed = False
                continue
            ir = ir_path.read_text(encoding="utf-8")
            word = 32 if "-p:32:32" in ir else 64
            musttail = ir.count("musttail call")
            jumps = tail_jumps(asm_path.read_text(encoding="utf-8"))
            # O2 may inline an entry into its caller, so it can only have fewer sites, never a plain call.
            ok = musttail == expected if level == "-O0" else 0 < musttail <= expected
            print(f"{target:32} {level} word=i{word} musttail={musttail} "
                  f"tail_jumps={jumps} {'ok' if ok else 'FAIL'}")
            passed = passed and ok
    return passed


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang")
    parser.add_argument("--out")
    args = parser.parse_args()
    clang = find_clang(args.clang)
    out = Path(args.out) if args.out else Path(tempfile.mkdtemp(prefix="execution-model-"))
    out.mkdir(parents=True, exist_ok=True)
    print(f"clang: {run([clang, '--version'])[1].splitlines()[0]}\nout: {out}\n")
    passed = host_runs(clang, out)
    passed = target_checks(clang, out) and passed
    print("PASS" if passed else "FAIL")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())

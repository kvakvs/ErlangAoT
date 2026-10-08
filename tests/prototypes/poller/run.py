"""Build and run the step-57A poller prototype on this host and, with --wsl, under WSL Linux.

Usage: python run.py [--clang PATH] [--wsl]. Not part of CTest; results are recorded in docs/ports.md#io-thread.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent


def find_clang(explicit):
    """Use --clang, else clang++ on PATH, else the default Windows LLVM install."""
    candidates = [explicit, shutil.which("clang++"),
                  os.path.join(os.environ.get("ProgramFiles", ""), "LLVM", "bin", "clang++.exe")]
    for candidate in candidates:
        if candidate and os.path.isfile(candidate):
            return candidate
    sys.exit("clang++ not found; pass --clang")


def run(command):
    """Run a command, echo its output and return its exit status."""
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", errors="replace")
    sys.stdout.write(result.stdout + result.stderr)
    return result.returncode


def host(clang):
    """Build and run on this host."""
    with tempfile.TemporaryDirectory() as work:
        exe = Path(work) / ("poller.exe" if os.name == "nt" else "poller")
        if run([clang, "-std=c++23", "-O1", str(HERE / "poller.cpp"), "-o", str(exe)]) != 0:
            return 1
        return run([str(exe)])


def wsl():
    """Build and run under WSL with its own clang++ and pthreads."""
    source = subprocess.run(["wsl", "-e", "wslpath", "-a", str(HERE / "poller.cpp")], capture_output=True,
                            text=True).stdout.strip()
    script = f'clang++ -std=c++23 -O1 -pthread "{source}" -o /tmp/clause_poller && /tmp/clause_poller'
    return run(["wsl", "-e", "bash", "-c", script])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--clang")
    parser.add_argument("--wsl", action="store_true", help="also build and run under WSL Linux")
    args = parser.parse_args()
    status = host(find_clang(args.clang))
    if args.wsl:
        status |= wsl()
    sys.exit(status)


if __name__ == "__main__":
    main()

"""Run the program fixtures and selected executable goldens linked with --lto, and record sizes and link times
next to plain -O2 (step 62). Sizes and times are descriptive, never thresholds."""
import concurrent.futures
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

tool, root, directory, suffix = sys.argv[1:]
source, work = Path(root), Path(directory)
shutil.rmtree(work, ignore_errors=True)
work.mkdir(parents=True)
runner = source / "tests/compiler/executables/run.py"
# Program fixtures (multi-module projects, processes, library modules) plus executables covering records, funs,
# dynamic calls, exceptions, receive and the step-59 proofs.
CASES = [*sorted((source / "tests/fixtures/programs").glob("*/fixture.json"))]
CASES = [path.parent for path in CASES] + [source / "tests/fixtures/executables" / name for name in (
    "demo", "proofs", "closures", "dynamic_calls", "try_catch", "native_records", "selective_receive", "tail_calls")]


def golden_run(case):
    """Link and run one case with -O2 --lto through both drivers against its golden; returns (case, ok, output)."""
    result = subprocess.run([sys.executable, str(runner), tool, str(work / "runs" / case.name), str(case),
                             f"--suffix={suffix}", "--lto"], capture_output=True, text=True, timeout=900)
    return case.name, result.returncode == 0, result.stdout + result.stderr


def measure(lto):
    """Link the demo with -O2, with or without --lto: executable bytes and link seconds."""
    name = "demo-lto" if lto else "demo-o2"
    example = source / "examples/compile"
    start = time.perf_counter()
    result = subprocess.run([tool, "-O2", *(["--lto"] if lto else []), "-o", name, str(example / "answer.erl"),
                             str(example / "client.erl")], cwd=work, capture_output=True, timeout=300)
    seconds = time.perf_counter() - start
    assert result.returncode == 0 and not result.stderr, result
    return {"bytes": (work / (name + suffix)).stat().st_size, "link_seconds": round(seconds, 3)}


with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
    outcomes = list(pool.map(golden_run, CASES))
failures = [name for name, ok, _ in outcomes if not ok]
for name, ok, output in outcomes:
    if not ok:
        print(output)
report = {"timings_are_not_thresholds": True, "cases": len(CASES), "O2": measure(False), "O2-lto": measure(True)}
(work / "lto.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
print(json.dumps(report, indent=2))
assert not failures, failures
print(f"lto: {len(CASES)} cases pass their goldens with --lto")


def rejected(status, arguments, message):
    """A refused --lto use: a usage error (2) or a link failure (1), the message on stderr, no executable."""
    result = subprocess.run([tool, *arguments], cwd=work, capture_output=True, text=True, timeout=120)
    assert result.returncode == status and message in result.stderr, (arguments, result)


demo = [str(source / "examples/compile/answer.erl"), str(source / "examples/compile/client.erl")]
rejected(2, ["--lto", *demo], "--lto requires --output or a linking project build")
rejected(2, ["--lto", "--emit", "obj", *demo], "--lto requires --output")
rejected(1, ["--lto", "--target-triple", "arm64-apple-macosx", "-o", "mac", *demo], "--lto is not supported for target")
assert not list(work.glob("mac*")) and not list(work.glob(".clause-link*"))
print("lto: unsupported uses are refused")

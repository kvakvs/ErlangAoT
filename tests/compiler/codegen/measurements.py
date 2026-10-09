"""Record noisy cost measurements separately from deterministic specialization assertions."""
import json
import pathlib
import re
import sys
import time
from differential import generate, run


def wide_source(work, expected):
    """Stress a 255-argument contract without expanding the union's Cartesian product."""
    path = work / "answer.erl"
    text = path.read_text(encoding="utf-8")
    text = text.replace("-module(answer).", "-module(answer).\n-export([wide/255]).")
    text += "\n-type wide() :: " + " | ".join(str(i) for i in range(64)) + ".\n"
    text += "-spec wide(" + ",".join(["wide()"] * 255) + ") -> wide().\n"
    text += "wide(" + ",".join(f"X{i}" for i in range(255)) + ") -> X127.\n"
    path.write_text(text, encoding="utf-8")
    with (work / "calls.txt").open("a", encoding="utf-8") as calls:
        calls.write("answer wide 255 " + " ".join(str(i % 64) for i in range(255)) + "\n")
    return expected + "63\n"


def measure_source(tool, work, mode, options, executable, expected):
    """Measure real CLI outputs and process execution, including startup and text I/O."""
    record = {"mode": mode, "variants": 0}
    for kind in ["llvm-ir", "obj"]:
        root = work / mode / kind
        start = time.perf_counter()
        run([tool, *options, "--emit", kind, "--artifact-dir", str(root),
             str(work / "answer.erl"), str(work / "client.erl")])
        record[kind + "_compile_seconds"] = time.perf_counter() - start
        outputs = sorted(root.iterdir())
        record[kind + "_bytes"] = sum(path.stat().st_size for path in outputs)
        # Annotated IR text of this call-dense client grows with explicit-frame transfers (a resume block per call).
        assert len(outputs) == 2 and record[kind + "_bytes"] < (2 if kind == "llvm-ir" else 1) * 1024 * 1024
        if kind == "llvm-ir":
            ir = "\n".join(path.read_text(encoding="utf-8") for path in outputs)
            record["variants"] = len(re.findall(r"^define .*@[^\n]*\.type", ir, re.MULTILINE))
            record["proven_reads"] = ir.count("proven.word")
    start = time.perf_counter()
    result = run([str(executable)], input=(work / "calls.txt").read_text(encoding="utf-8"))
    record["execution_seconds"] = time.perf_counter() - start
    assert result.stdout == expected and not result.stderr
    return record


def main():
    """Build native policy variants, run them, and retain machine-readable evidence."""
    tool, cmake, root, directory, settings, config, suffix, emitter = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    expected = wide_source(work, generate(source, work))
    (work / "expected.txt").write_text(expected, encoding="utf-8")
    records = []
    synthetic = []
    for mode in ["O0", "O2-disabled", "O2"]:
        level = "O0" if mode == "O0" else "O2"
        extra = "--no-type-specialization" if mode == "O2-disabled" else ""
        common = [f"-DSOURCE_ROOT={source.as_posix()}", f"-DHOST_SETTINGS={settings}",
                  f"-DHOST_CONFIG={config}", f"-DHOST_SUFFIX={suffix}"]
        run([cmake, f"-DTOOL={tool}", f"-DOPTIMIZATION={level}", f"-DEXTRA_OPTIONS={extra}",
             f"-DINPUT_ROOT={work.as_posix()}", f"-DTEST_DIR={(work / mode).as_posix()}", *common,
             "-P", str(source / "tests/compiler/codegen/native.cmake")])
        executable = work / mode / "build/bin" / config / ("linked" + suffix)
        records.append(measure_source(tool, work, mode, ["-" + level] + ([extra] if extra else []),
                                      executable, expected))
        synthetic_root = work / ("synthetic-" + mode)
        start = time.perf_counter()
        result = run([cmake, f"-DEMITTER={emitter}", f"-DOPTIMIZATION={mode}",
                      f"-DCONSUMER={(source / 'tests/compiler/codegen/specialization_consumer.cpp').as_posix()}",
                      f"-DTEST_DIR={synthetic_root.as_posix()}", *common,
                      "-P", str(source / "tests/compiler/codegen/registration.cmake")])
        synthetic.append({"mode": mode, "build_and_check_seconds": time.perf_counter() - start,
                          "pre_optimization_metrics": re.findall(r"bits=.*", result.stdout),
                          "object_bytes": sum(p.stat().st_size for p in (synthetic_root / "source").glob("*.obj"))})
    # Since step 59 inferred proofs change O2 code; every policy still prints the expected results (checked above).
    report = {"timings_are_not_thresholds": True, "source": records, "synthetic": synthetic}
    (work / "measurements.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

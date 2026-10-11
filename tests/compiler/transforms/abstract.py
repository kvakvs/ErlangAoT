"""Compare clau --print-abstr with OTP's epp forms for every fixture, from its source and from the golden itself as a
.abstr input, which must print back unchanged (added for parse transforms)."""
import pathlib
import subprocess
import sys


def main():
    """abstract.py CLAU COMPARE FIXTURES WORK: one golden per fixture module, compared as terms."""
    tool, compare, fixtures, work = (pathlib.Path(argument).resolve() for argument in sys.argv[1:])
    work.mkdir(parents=True, exist_ok=True)
    failures = []
    for golden, source in [(golden, name) for golden in sorted(fixtures.glob("*.abstr"))
                           for name in (golden.with_suffix(".erl").name, golden.name)]:
        result = subprocess.run([str(tool), "--print-abstr", source], cwd=fixtures, capture_output=True,
                                timeout=60, check=False)
        actual = work / (source + ".out")
        actual.write_bytes(result.stdout)
        if result.returncode != 0:
            failures.append(f"{source}: exit {result.returncode}\n{result.stderr.decode('utf-8', 'replace')}")
            continue
        check = subprocess.run([str(compare), str(golden), str(actual)], capture_output=True, text=True,
                               encoding="utf-8", timeout=60, check=False)
        if check.returncode != 0:
            failures.append(f"{source}: {check.stderr}")
    if failures:
        print("\n".join(failures), file=sys.stderr)
        sys.exit(1)
    print("transforms_abstract: ok")


if __name__ == "__main__":
    main()

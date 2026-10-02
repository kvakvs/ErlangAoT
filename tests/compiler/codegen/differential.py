"""Compare deterministic accepted Erlang programs through OTP and the public CLI."""
import pathlib
import random
import subprocess
import sys


def run(command, **kwargs):
    """Keep command, status and both streams in any failure diagnostic."""
    result = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                            timeout=180, **kwargs)
    if result.returncode:
        raise AssertionError(f"{command}: {result.returncode}\n{result.stdout}\n{result.stderr}")
    return result


def expression(rng, depth):
    """Generate a bounded tree together with an independent integer evaluator."""
    if depth == 0:
        choice = rng.randrange(3)
        if choice < 2:
            return ("X", "Y")[choice], lambda args: args[choice]
        value = rng.randint(-100000, 100000)
        return str(value), lambda args: value
    left, evaluate_left = expression(rng, depth - 1)
    right, evaluate_right = expression(rng, depth - 1)
    name = rng.choice(["first", "second"])
    return (f"answer:{name}(answer:identity({left}), {right})",
            evaluate_left if name == "first" else evaluate_right)


def generate(source, work):
    """Pair annotated/unannotated functions and deliberately false contracts."""
    fixtures = source / "tests/fixtures/codegen/native"
    answer = (fixtures / "answer.erl").read_text(encoding="utf-8")
    answer += "\n-spec value() -> atom().\n-spec identity(integer()) -> integer().\n"
    (work / "answer.erl").write_text(answer, encoding="utf-8")
    client = (fixtures / "client.erl").read_text(encoding="utf-8")
    calls = (fixtures / "calls.txt").read_text(encoding="utf-8")
    expected = (fixtures / "expected.txt").read_text(encoding="utf-8").splitlines()
    rng = random.Random(290041)
    bodies = []
    exports = []
    for index in range(24):
        body, evaluate = expression(rng, 1 + index % 4)
        for prefix in ["plain", "typed"]:
            name = f"{prefix}{index}"
            exports.append(name + "/2")
            if prefix == "typed":
                bodies.append(f"-spec {name}(integer(), integer()) -> integer().")
            bodies.append(f"{name}(X, Y) -> {body}.")
            for args in [(-134217728, 134217727), (0, -1), (17, 29)]:
                calls += f"client {name} 2 {args[0]} {args[1]}\n"
                expected.append(str(evaluate(args)))
    # Erlang requires exports before function definitions.
    client = client.replace("-module(client).", "-module(client).\n-export([" + ",".join(exports) + "]).")
    (work / "client.erl").write_text(client + "\n" + "\n".join(bodies) + "\n", encoding="utf-8")
    (work / "calls.txt").write_text(calls, encoding="utf-8")
    return "\n".join(expected) + "\n"


def main():
    """Compare retained OTP results in each native mode through the shared harness."""
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    sys.path.insert(0, str(source / 'tests/compiler/patternmatch'))
    from stored import load
    load(source, 'differential', work)
    expected = (work / 'expected.txt').read_text(encoding='utf8')
    for mode, level, extra in [("O0", "O0", ""), ("O0-disabled", "O0", "--no-type-specialization"),
                               ("O2-disabled", "O2", "--no-type-specialization"), ("O2", "O2", "")]:
        run([cmake, f"-DTOOL={tool}", f"-DOPTIMIZATION={level}", f"-DEXTRA_OPTIONS={extra}",
             f"-DSOURCE_ROOT={source.as_posix()}", f"-DTEST_DIR={(work / mode).as_posix()}",
             f"-DINPUT_ROOT={work.as_posix()}",
             f"-DHOST_SETTINGS={settings}", f"-DHOST_CONFIG={config}", f"-DHOST_SUFFIX={suffix}",
             "-P", str(source / "tests/compiler/codegen/native.cmake")])
    print(f"{len(expected.splitlines())} calls agree with OTP in four modes, each executed twice")


if __name__ == "__main__":
    main()

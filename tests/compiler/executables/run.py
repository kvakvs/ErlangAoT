"""Link one executable golden case under every test policy, run it and compare with its OTP golden."""
import argparse
import concurrent.futures
import difflib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import cases

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'patternmatch'))
import matrix  # noqa: E402  (shared fast/full policy selection)

# Policy combinations linked and run at once; each is independent, so a case takes about one combination's time.
COMBINATION_JOBS = 4


def manifest(case, entry, output):
    """Schema-1 project manifest that links the staged sources as one executable target."""
    return (f'schema_version = 1\n\n[[targets]]\nname = "{case}"\nsource_dirs = ["src"]\n'
            f'entry = "{entry}"\noutput = "{output}"\n')


def command(tool, work, case, golden, policy):
    """Compiler invocation for one policy; returns its label and argument list."""
    level, extra, name, project = policy
    label = f'{name}-{"project" if project else "positional"}'
    options = [f'-{level}', *([extra] if extra else [])]
    output = f'{label}/{case}'
    if project:
        (work / f'{label}.toml').write_bytes(manifest(case, golden['entry'], output).encode())
        return label, [tool, '--project', f'{label}.toml', *options]
    (work / label).mkdir()
    inputs = sorted(f'src/{path.name}' for path in (work / 'src').glob('*.erl'))
    return label, [tool, *options, '--entry', golden['entry'], '-o', output, *inputs]


def text(data):
    """Decodes captured output with LF line ends."""
    return data.replace(b'\r\n', b'\n').decode('utf8', errors='replace')


def stdout_problem(expected, actual):
    """Describes a stdout mismatch as a unified diff, or None when the text is identical."""
    if expected == actual:
        return None
    lines = list(difflib.unified_diff(expected.splitlines(), actual.splitlines(),
                                      'expected stdout (golden)', 'actual stdout', lineterm=''))
    if not lines:
        return f'stdout differs only in line breaks: expected {expected!r}, got {actual!r}'
    return 'stdout differs:\n' + '\n'.join(lines)


def environment(run):
    """The host environment without inherited runtime flags, plus the run's own 'env' entries."""
    inherited = {name: value for name, value in os.environ.items() if name.upper() != 'ERLANG_AOT_FLAGS'}
    return inherited | run.get('env', {})


def variants(golden):
    """Every run, once per scheduler count the golden lists under 'workers', else once with the default count."""
    for run in golden['runs']:
        if 'workers' not in golden:
            yield run, ''
        for count in golden.get('workers', []):
            flags = ' '.join(filter(None, [run.get('env', {}).get('ERLANG_AOT_FLAGS'), f'--schedulers {count}']))
            yield run | {'env': run.get('env', {}) | {'ERLANG_AOT_FLAGS': flags}}, f' workers={count}'


def compare(executable, run):
    """Runs one golden invocation; returns readable mismatch descriptions (empty when it matches)."""
    try:
        result = subprocess.run([executable, *run['args']], capture_output=True, timeout=60, check=False,
                                env=environment(run))
    except subprocess.TimeoutExpired:
        return ['timed out after 60 s']
    stdout, stderr = text(result.stdout), text(result.stderr)
    problems = []
    if result.returncode != run['exit_status']:
        problems.append(f'exit status: expected {run["exit_status"]}, got {result.returncode}')
    if problem := stdout_problem(run['stdout'], stdout):
        problems.append(problem)
    if not re.search(run['stderr'], stderr):
        problems.append(f'stderr does not match /{run["stderr"]}/')
    if problems and stderr:
        problems.append('stderr:\n' + stderr.rstrip('\n'))
    return problems


def check_policy(tool, work, case, golden, policy, suffix):
    """Links the case under one policy and runs every golden invocation; returns the failure count and the report
    lines, printed by the caller so concurrent combinations do not interleave."""
    label, arguments = command(tool, work, case, golden, policy)
    result = subprocess.run(arguments, cwd=work, capture_output=True, timeout=300, check=False)
    if result.returncode != 0:
        return 1, [f'FAIL {case} [{label}]: compiler exited {result.returncode}\n{text(result.stderr)}']
    executable = work / f'{label}/{case}{suffix}'
    failures, lines = 0, []
    for run, variant in variants(golden):
        problems = compare(executable, run)
        status = 'FAIL' if problems else 'ok'
        lines.append(f'{status} {case} [{label}] args={json.dumps(run["args"])}{variant}')
        lines.extend('    ' + problem.replace('\n', '\n    ') for problem in problems)
        failures += bool(problems)
    return failures, lines


def main():
    """Checks one case directory against its golden under the fast or full policy matrix."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tool', help='erlangaot executable')
    parser.add_argument('work', type=pathlib.Path, help='scratch directory, recreated')
    parser.add_argument('case', type=pathlib.Path, help='case directory holding golden.json')
    parser.add_argument('--suffix', default='', help='host executable suffix appended by the linker')
    options = parser.parse_args()
    case_dir, work, tool = options.case.resolve(), options.work.resolve(), str(pathlib.Path(options.tool).resolve())
    golden = cases.verify(case_dir)
    shutil.rmtree(work, ignore_errors=True)
    cases.stage(case_dir, golden, work / 'src')
    policies = matrix.combinations()
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(COMBINATION_JOBS, len(policies))) as pool:
        outcomes = list(pool.map(lambda policy: check_policy(tool, work, case_dir.name, golden, policy, options.suffix),
                                 policies))
    failures = 0
    for count, lines in outcomes:
        print('\n'.join(lines))
        failures += count
    print(f'{case_dir.name}: {failures} failure(s)')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()

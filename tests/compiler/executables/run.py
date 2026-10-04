"""Link one executable golden case under every test policy, run it and compare with its OTP golden."""
import argparse
import difflib
import json
import pathlib
import re
import shutil
import subprocess
import sys
import cases

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'patternmatch'))
import matrix  # noqa: E402  (shared fast/full policy selection)


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


def compare(executable, run):
    """Runs one golden invocation; returns readable mismatch descriptions (empty when it matches)."""
    try:
        result = subprocess.run([executable, *run['args']], capture_output=True, timeout=60, check=False)
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
    """Links the case under one policy and runs every golden invocation; returns the failure count."""
    label, arguments = command(tool, work, case, golden, policy)
    result = subprocess.run(arguments, cwd=work, capture_output=True, timeout=300, check=False)
    if result.returncode != 0:
        print(f'FAIL {case} [{label}]: compiler exited {result.returncode}\n{text(result.stderr)}')
        return 1
    executable = work / f'{label}/{case}{suffix}'
    failures = 0
    for run in golden['runs']:
        problems = compare(executable, run)
        status = 'FAIL' if problems else 'ok'
        print(f'{status} {case} [{label}] args={json.dumps(run["args"])}')
        for problem in problems:
            print('    ' + problem.replace('\n', '\n    '))
        failures += bool(problems)
    return failures


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
    failures = sum(check_policy(tool, work, case_dir.name, golden, policy, options.suffix)
                   for policy in matrix.combinations())
    print(f'{case_dir.name}: {failures} failure(s)')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()

"""Runner self-check: a wrong golden fails with a readable diff and a stale golden fails before linking."""
import argparse
import json
import os
import pathlib
import shutil
import subprocess
import sys
import cases

RUNNER = pathlib.Path(__file__).resolve().parent / 'run.py'


def broken_case(work):
    """Copies the demo sources beside a regenerated-looking golden whose first stdout line is wrong."""
    demo = cases.CASES / 'demo'
    golden = cases.load(demo)
    case_dir = work / 'broken'
    shutil.rmtree(case_dir, ignore_errors=True)
    case_dir.mkdir(parents=True)
    for path in cases.sources(demo, golden).values():
        shutil.copyfile(path, case_dir / path.name)
    del golden['sources']
    golden['inputs'] = cases.inputs(case_dir, golden)
    golden['runs'][0]['stdout'] = golden['runs'][0]['stdout'].replace('42\n', '43\n', 1)
    (case_dir / cases.GOLDEN).write_bytes(json.dumps(golden, indent=2).encode())
    return case_dir


def run_runner(tool, work, case_dir, suffix):
    """Runs the real runner in fast mode (two policies) and returns its exit status and output."""
    environment = dict(os.environ, CLAUSE_TEST_MODE='fast')
    result = subprocess.run([sys.executable, str(RUNNER), tool, str(work), str(case_dir), f'--suffix={suffix}'],
                            capture_output=True, text=True, env=environment, timeout=600, check=False)
    return result.returncode, result.stdout + result.stderr


def expect(name, status, output, fragments):
    """Requires a runner failure whose output contains every fragment."""
    print(f'--- {name}: runner exit {status} ---\n{output}')
    missing = [fragment for fragment in fragments if fragment not in output]
    if status != 1 or missing:
        sys.exit(f'{name}: expected runner exit 1 with output containing {missing}')


def main():
    """Checks both failure reports against a disposable copy of the demo case."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tool', help='clau executable')
    parser.add_argument('work', type=pathlib.Path, help='scratch directory')
    parser.add_argument('--suffix', default='', help='host executable suffix appended by the linker')
    options = parser.parse_args()
    case_dir = broken_case(options.work)
    status, output = run_runner(options.tool, options.work / 'run', case_dir, options.suffix)
    expect('wrong golden', status, output, ['FAIL broken [O0-positional] args=[]', '--- expected stdout (golden)',
                                            '+++ actual stdout', '\n    -43\n    +42\n'])
    with (case_dir / 'answer.erl').open('ab') as source:
        source.write(b'%% edited after generation\n')
    status, output = run_runner(options.tool, options.work / 'run', case_dir, options.suffix)
    expect('stale golden', status, output, ['broken: stale golden: sources changed since OTP generation'])


if __name__ == '__main__':
    main()

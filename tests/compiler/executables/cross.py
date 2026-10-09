"""Run every executable golden case and program fixture for another target (docs/validation.md#other-targets): this
host's clau compiles and links with --target-triple, a runtime library built for that target and a linker that
reaches its system libraries; the executables run natively or through the emulator the host starts for them."""
import argparse
import concurrent.futures
import os
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[3]
RUNNER = pathlib.Path(__file__).resolve().parent / 'run.py'


def cases(pattern):
    """Executable case directories (golden.json), then program fixtures (fixture.json), whose names match the regular
    expression `pattern`."""
    executables = sorted(path.parent for path in (ROOT / 'tests/fixtures/executables').glob('*/golden.json'))
    programs = sorted(path.parent for path in (ROOT / 'tests/fixtures/programs').glob('*/fixture.json'))
    return [case for case in executables + programs if re.search(pattern, case.name)]


def check(case, options):
    """Runs one case through run.py with the target's options; returns its exit status and output."""
    target = ['--target-triple', options.target_triple, '--runtime-library', options.runtime_library]
    if options.linker:
        target += ['--linker', options.linker]
    word_bits = 32 if options.target_triple.startswith(('i386', 'i686', 'arm-', 'armv7')) else 64
    arguments = [sys.executable, str(RUNNER), options.tool, str(options.work / case.name), str(case),
                 f'--suffix={options.suffix}', f'--word-bits={word_bits}', f'--run-timeout={options.run_timeout}',
                 *(f'--target-option={part}' for part in target)]
    result = subprocess.run(arguments, capture_output=True, text=True, errors='replace', check=False)
    return result.returncode, result.stdout + result.stderr


def main():
    """Checks the selected cases a few at a time and prints each failing case's report and a summary."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('tool', help='clau executable of this host')
    parser.add_argument('work', type=pathlib.Path, help='scratch directory')
    parser.add_argument('--target-triple', required=True)
    parser.add_argument('--runtime-library', required=True, help='clause_runtime archive built for the target')
    parser.add_argument('--linker', default='', help='Clang driver (or wrapper) that links for the target')
    parser.add_argument('--suffix', default='', help='executable suffix of the target (.exe for Windows)')
    parser.add_argument('--jobs', type=int, default=max(1, (os.cpu_count() or 4) // 8),
                        help='cases at once; each links and runs four combinations at once')
    parser.add_argument('--run-timeout', type=int, default=60, help='seconds each run may take (raise under emulation)')
    parser.add_argument('--case', default='', help='only cases whose name matches this regular expression')
    options = parser.parse_args()
    # Each case compiles in its own directory: paths become absolute (a bare linker name stays a PATH lookup).
    options.work = options.work.resolve()
    options.runtime_library = str(pathlib.Path(options.runtime_library).resolve())
    if os.sep in options.linker or '/' in options.linker:
        options.linker = str(pathlib.Path(options.linker).resolve())
    selected = cases(options.case)
    with concurrent.futures.ThreadPoolExecutor(max_workers=options.jobs) as pool:
        outcomes = list(pool.map(lambda case: (case.name, *check(case, options)), selected))
    failed = [(name, output) for name, status, output in outcomes if status != 0]
    for _, _, output in outcomes:
        for line in output.splitlines():
            if line.startswith('skip '):
                print(line)
    for name, output in failed:
        print(f'===== {name}\n{output}')
    print(f'{options.target_triple}: {len(selected) - len(failed)}/{len(selected)} cases pass'
          + (f'; failing: {", ".join(name for name, _ in failed)}' if failed else ''))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()

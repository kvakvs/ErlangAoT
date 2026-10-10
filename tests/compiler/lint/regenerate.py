"""Explicitly regenerate, or check, lint case goldens with an installed OTP (tests/fixtures/lint/README.md)."""
import argparse
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import lint

ORACLE = pathlib.Path(__file__).with_name('oracle.escript')


def reference():
    """Reads the pinned maint-29 revision recorded alongside the oracle version."""
    text = (lint.ROOT / 'references/otp-pin.cmake').read_text(encoding='utf8')
    branch = re.search(r'REFERENCE_BRANCH "([^"]+)"', text).group(1)
    return {'branch': branch, 'revision': re.search(r'REFERENCE_REVISION "([0-9a-f]{40})"', text).group(1)}


def regenerate(escript, case, work):
    """Compiles one case under OTP; returns the committed golden and the freshly generated one."""
    golden = lint.load(case)
    output = work / f'{case}.json'
    result = subprocess.run([escript, str(ORACLE), str(output), *golden['files']], cwd=lint.CASES / case,
                            capture_output=True, timeout=300, check=False)
    if result.returncode != 0 or not output.exists():
        sys.exit(f'{case}: oracle failed: {result.stderr.decode(errors="replace")}')
    observed = json.loads(output.read_text(encoding='utf8'))
    diagnostics = [{key: item[key] for key in ('file', 'line', 'column', 'severity', 'message')}
                   for item in sorted(observed['diagnostics'], key=lambda d: tuple(lint.normalized([d])[0]))]
    return golden, {'schema': 1, 'files': golden['files'], 'exit_status': observed['exit_status'],
                    'diagnostics': diagnostics, 'oracle_version': observed['version'], 'reference': reference(),
                    'inputs': lint.inputs(case, golden)}


def refresh(escript, case, work, check):
    """Writes one golden, or in check mode reports whether OTP still reproduces it."""
    golden, fresh = regenerate(escript, case, work)
    count = len(fresh['diagnostics'])
    if check:
        same = golden == fresh
        print(f'{case}: {"reproduced" if same else "DRIFT"} ({count} diagnostics)')
        return same
    (lint.CASES / case / lint.GOLDEN).write_bytes((json.dumps(fresh, indent=2, ensure_ascii=False) + '\n').encode())
    print(f'{case}: wrote {count} diagnostics')
    return True


def main():
    """Parses options and refreshes or checks every selected case."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--escript', required=True, help='OTP 29 escript executable')
    parser.add_argument('--case', action='append', choices=lint.names(), help='limit to one case')
    parser.add_argument('--check', action='store_true', help='compare with committed goldens, write nothing')
    options = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='lint-oracle-') as work:
        results = [refresh(options.escript, case, pathlib.Path(work), options.check)
                   for case in options.case or lint.names()]
    sys.exit(0 if all(results) else 1)


if __name__ == '__main__':
    main()

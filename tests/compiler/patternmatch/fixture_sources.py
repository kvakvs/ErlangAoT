"""Check that normal fixtures stage only local source and retained OTP observations."""
import pathlib
import re
import sys
from authored import stage
from regenerate import CORPORA
from stored import load


def main():
    """Exercise source inventory and hash verification without an OTP checkout or executable."""
    source, work = map(pathlib.Path, sys.argv[1:])
    marker = re.compile(rb'Copyright Ericsson|%CopyrightBegin%|SPDX-License-Identifier: Apache-2\.0')
    for directory in ['compiler', 'runtime', 'abi', 'examples', 'tests']:
        for path in (source / directory).rglob('*'):
            if path.suffix in ['.erl', '.hrl', '.cpp', '.hpp', '.escript']:
                assert not marker.search(path.read_bytes()), path
    generated = source / 'tests/fixtures/patternmatch/generated'
    assert not list(generated.glob('*/*.erl')) and not list(generated.glob('*/*.hrl'))
    for name in CORPORA:
        staged, retained = work / name / 'inputs', work / name / 'retained'
        staged.mkdir(parents=True, exist_ok=True)
        stage(source, name, staged)
        assert not (staged / 'expected.txt').exists(), name
        load(source, name, retained)
        for path in staged.iterdir():
            assert path.read_bytes() == (retained / path.name).read_bytes(), path
    print(f'{len(CORPORA)} local source inventories and observation manifests passed without OTP.')


if __name__ == '__main__':
    main()

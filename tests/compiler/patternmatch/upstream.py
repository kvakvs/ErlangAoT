"""Optional upstream provenance, grammar, catalog and golden-fixture drift audit."""
import pathlib
import sys
import evidence
import patterns
import records
from stored import load


def main():
    """Keep reference-dependent checks explicit and separate from the normal test suite."""
    tool, root, otp_root, directory, escript = sys.argv[1:]
    source, otp, work = pathlib.Path(root), pathlib.Path(otp_root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    fixtures = source / 'tests/fixtures/patternmatch'
    evidence.provenance(source, otp, fixtures, work)
    evidence.verify_manifest(otp, (fixtures / 'pattern-otp.tsv').read_text(encoding='utf8'))
    evidence.catalog(otp, fixtures)
    evidence.suites(tool, otp, work)
    patterns.catalog(source, otp)
    patterns.suites(tool, otp)
    records.suites(tool, otp)
    load(source, 'baseline', work)
    print(evidence.run([escript, str(source / 'tests/compiler/patternmatch/oracle.escript'),
                       str(fixtures), str(work)]))
    print(evidence.run([sys.executable, str(source / 'tests/compiler/patternmatch/regenerate.py'),
        '--otp', str(otp), '--escript', escript, '--check']))


if __name__ == '__main__':
    main()

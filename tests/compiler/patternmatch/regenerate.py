"""Explicitly regenerate reviewed, committed native-test inputs and expected results using OTP."""
import argparse
import importlib
import hashlib
import json
import pathlib
import tempfile
from evidence import digest, provenance, run

CORPORA = ['atoms', 'immediate', 'services', 'booleans', 'clauses', 'sequences', 'containers',
           'integers', 'floats', 'maps', 'bits', 'records', 'guard_catalog', 'facts', 'bindings', 'patterns', 'baseline', 'differential']


def contents(path):
    """Preserve source text with LF; omit insignificant trailing separators in native call records."""
    data = path.read_bytes().replace(b'\r\n', b'\n')
    if path.name == 'calls.txt':
        data = b'\n'.join(line.rstrip(b' \t') for line in data.splitlines()) + b'\n'
    return data


def resolution(source, escript, work):
    """Record OTP acceptance for the project-owned guard-resolution cases at regeneration time."""
    fixture = source / 'tests/fixtures/patternmatch/guard-resolution.json'
    cases = work / 'resolution'
    cases.mkdir()
    terms = []
    for row in json.loads(fixture.read_text(encoding='utf8')):
        text = f'-module({row["name"]}).\n{row.get("metadata", "")}\n{row["body"]}\n'
        (cases / (row['name'] + '.erl')).write_bytes(text.encode())
        terms.append(f'{{{row["name"]},{"rejected" if row["diagnostic"] else "accepted"},none}}.')
    (cases / 'patterns.term').write_bytes(('\n'.join(terms) + '\n').encode())
    oracle = run([escript, str(source / 'tests/compiler/patternmatch/patterns.escript'), str(cases)])
    return {'fixture_sha256': digest(fixture), 'oracle': oracle}


def generate(source, otp, escript, name, work):
    """Preserve complete helpers/adaptation metadata and obtain expected values solely from OTP."""
    if name in ['bindings', 'patterns', 'baseline', 'differential']:
        from regenerate_cases import generate as generate_cases
        return generate_cases(source, otp, escript, name, work)
    module = importlib.import_module(name)
    if name == 'atoms':
        module.fixtures(source, otp, work)
        evidence = json.loads((work / 'helpers.json').read_text(encoding='utf8'))
    elif name == 'immediate':
        evidence = module.prepare(source, otp, work)
    elif name == 'booleans':
        evidence = module.kernels(source, otp, work)
    else:
        evidence = module.kernels(otp, work)
    if name in ['records', 'guard_catalog']:
        evidence['semantic_oracle'] = run([escript, str(source / 'tests/compiler/patternmatch/patterns.escript'), str(work)])
    oracle = 'atoms.escript' if name == 'atoms' else 'immediate.escript'
    output = run([escript, str(source / 'tests/compiler/patternmatch' / oracle), str(work)])
    (work / 'expected.txt').write_bytes(output.replace('\r\n', '\n').encode())
    if name != 'atoms':
        assert len(output.splitlines()) == evidence['calls']
    if name == 'services':
        evidence['resolution_oracle'] = resolution(source, escript, work)
    return evidence, oracle


def publish(source, name, work, evidence, oracle, version, check):
    """Publish normalized text plus source, oracle, generator and fixture hashes for review."""
    target = source / 'tests/fixtures/patternmatch/generated' / name
    filenames = sorted(path.name for path in work.iterdir()
                       if path.suffix in ['.erl', '.hrl', '.txt', '.term', '.toml'])
    if check:
        manifest = json.loads((target / 'manifest.json').read_text(encoding='utf8'))
        assert set(filenames) == set(manifest['files']), f'Changed fixture inventory: {name}'
        for filename in filenames:
            actual = hashlib.sha256(contents(work / filename)).hexdigest()
            assert actual == manifest['files'][filename], f'OTP fixture drift: {name}/{filename}'
        print(f'{name}: regenerated inputs/results match committed fixtures')
        return
    target.mkdir(parents=True, exist_ok=True)
    files = {}
    for filename in filenames:
        (target / filename).write_bytes(contents(work / filename))
        files[filename] = digest(target / filename)
    scripts = source / 'tests/compiler/patternmatch'
    generator = {'baseline': 'evidence.py', 'differential': '../codegen/differential.py'}.get(name, name + '.py')
    manifest = {'schema': 1, 'corpus': name, 'oracle_version': version,
        'provenance': json.loads((work / 'provenance.json').read_text(encoding='utf8')),
        'generator_sha256': {file: digest(scripts / file) for file in
            ['regenerate.py', 'regenerate_cases.py', generator, 'immediate.py', 'services.py', oracle,
             'patterns.escript', '../codegen/execution_oracle.escript']},
        'files': files, 'evidence': evidence}
    (target / 'manifest.json').write_bytes((json.dumps(manifest, indent=2) + '\n').encode())
    print(f'{name}: retained {len((work / "expected.txt").read_text(encoding="utf8").splitlines())} expected results')


def main():
    """Regeneration is opt-in and never runs from CTest or configuration."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--otp', type=pathlib.Path, required=True)
    parser.add_argument('--escript', required=True)
    parser.add_argument('--corpus', choices=CORPORA + ['all'], default='all')
    parser.add_argument('--check', action='store_true', help='compare regenerated data without updating fixtures')
    args = parser.parse_args()
    source = pathlib.Path(__file__).resolve().parents[3]
    version = run([args.escript, str(source / 'cmake/ErlangVersion.escript')]).strip().splitlines()
    for name in CORPORA if args.corpus == 'all' else [args.corpus]:
        work = pathlib.Path(tempfile.mkdtemp(prefix='oracle-' + name + '-', dir=source / 'build'))
        provenance(source, args.otp.resolve(), source / 'tests/fixtures/patternmatch', work)
        evidence, oracle = generate(source, args.otp.resolve(), args.escript, name, work)
        publish(source, name, work, evidence, oracle, version, args.check)


if __name__ == '__main__':
    main()

"""Exercise public IR inspection and validate each snapshot using the installed LLVM tools."""
from pathlib import Path
import itertools
import re
import shutil
import subprocess
import sys

tool, assembler, disassembler, directory, fixtures = sys.argv[1:]
work = Path(directory)
if work.exists():
    shutil.rmtree(work)
work.mkdir(parents=True)
(work / 'oracle').mkdir()
for name in ('answer.erl', 'client.erl'):
    shutil.copyfile(Path(fixtures) / name, work / name)
sequence = itertools.count()


def run(*args, expected=0):
    result = subprocess.run([tool, *args], cwd=work, capture_output=True, text=True, encoding='utf-8', timeout=30)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result


def snapshots(text):
    headers = list(re.finditer(r'^; erlangaot .* stage=(before|after)\n', text, re.MULTILINE))
    if not headers:
        return [('', text)]
    return [(header.group(0), text[header.end():headers[i + 1].start() if i + 1 < len(headers) else len(text)])
            for i, header in enumerate(headers)]


def round_trip(text):
    parts = snapshots(text)
    for _, assembly in parts:
        source = work / 'oracle' / f'{next(sequence)}.ll'
        source.write_text(assembly, encoding='utf-8')
        bitcode = source.with_suffix('.bc')
        subprocess.run([assembler, str(source), '-o', str(bitcode)], check=True, capture_output=True)
        subprocess.run([disassembler, str(bitcode), '-o', str(source.with_suffix('.roundtrip.ll'))], check=True, capture_output=True)
    return parts


for level in ('-O0', '-O2'):
    for flag in ('--print-ir', '--print-optimized-ir'):
        result = run(level, flag, '--verbose', 'answer.erl')
        assert not result.stdout.startswith('; erlangaot '), result.stdout
        assert len(round_trip(result.stdout)) == 1
        assert 'phase=lowering' in result.stderr and 'phase=emission' not in result.stderr
        assert ('phase=optimization' in result.stderr) == (flag == '--print-optimized-ir')
        assert re.search(r'define[^\n]*@eav1_616e73776572_6964656e74697479_1\(', result.stdout)
        assert '.register' in result.stdout

both = run('-O2', '--print-optimized-ir', '--print-ir', 'client.erl', 'answer.erl')
parts = round_trip(both.stdout)
assert len(parts) == 4
assert [re.search(r'module="([^"]+)"', head)[1] for head, _ in parts] == ['client', 'client', 'answer', 'answer']
assert [re.search(r'stage=(\w+)', head)[1] for head, _ in parts] == ['before', 'after', 'before', 'after']
assert 'eav1_616e73776572_70726976617465_0' in parts[2][1]
assert 'eav1_616e73776572_70726976617465_0' not in parts[3][1]
assert not both.stderr
normal = run('-O2', '--print-ir', '--print-optimized-ir', 'answer.erl').stdout
disabled = run('--no-type-specialization', '-O2', '--print-ir', '--print-optimized-ir', 'answer.erl').stdout
assert normal == disabled  # No removable representation checks exist in the supported source subset.
assert run('--print-ir', '--print-ir', 'answer.erl').stdout == run('--print-ir', 'answer.erl').stdout

(work / 'quoted.erl').write_text("-module('line\\nbreak'). value() -> 1.\n", encoding='utf-8')
quoted = run('--print-ir', '--print-optimized-ir', 'quoted.erl')
assert 'module="line\\x0abreak"' in quoted.stdout
round_trip(quoted.stdout)
shutil.copyfile(work / 'answer.erl', work / 'space å.erl')
round_trip(run('--print-ir', 'space å.erl').stdout)
(work / 'project.toml').write_text('''schema_version=1
[[targets]]
name="one"
sources=["answer.erl","client.erl"]
[[targets]]
name="two"
sources=["client.erl","answer.erl"]
''', encoding='utf-8')
project = run('--project', 'project.toml', '--target', 'two', '--target', 'one', '--print-ir', '--verbose')
project_parts = round_trip(project.stdout)
assert len(project_parts) == 4
assert ['target="two"' in h for h, _ in project_parts] == [True, True, False, False]
assert 'target="one"' in project_parts[-1][0]
assert 'phase=emission' not in project.stderr and '[comp]' in project.stderr

for flag in ('--print-ir', '--print-optimized-ir'):
    for other in (('--emit', 'obj'), ('--artifact-dir', 'out'), ('--output', 'exe'), ('--parse-check',),
                  ('--preprocess-check',), ('--print-pp',), ('--print-ast',)):
        rejected = run(flag, *other, 'missing.erl', expected=2)
        assert not rejected.stdout
    assert not run(flag, '--new-project', 'new', expected=2).stdout
    invalid = run(flag, '--target-triple', 'invalid', 'answer.erl', expected=1)
    assert not invalid.stdout
    (work / 'bad.erl').write_text('-module(bad). value() -> [1].\n', encoding='utf-8')
    assert not run(flag, 'answer.erl', 'bad.erl', expected=1).stdout
assert 'Usage:' in run('--help', '--print-ir', '--target-triple', 'invalid', 'missing.erl').stdout
assert not (work / 'build').exists() and not (work / 'out').exists() and not (work / 'new.toml').exists()

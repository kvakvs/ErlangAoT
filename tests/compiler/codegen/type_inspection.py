"""Inspect real source contracts and inferred facts without entering LLVM lowering."""
from pathlib import Path
import re
import shutil
import subprocess
import sys

tool, directory = sys.argv[1:]
work = Path(directory)
if work.exists():
    shutil.rmtree(work)
work.mkdir(parents=True)
owner = '''-module(owner).
-export([id/1, value/0, projection/2]).
-export_type([chain/1]).
-type chain(T) :: nil | {T, chain(T)}.
-opaque secret() :: integer().
-nominal nominal_id() :: integer().
-spec id(T) -> T when T :: term().
-spec value() -> atom().
-callback cb(integer()) -> integer().
-optional_callbacks([cb/1]).
id(X) -> X.
value() -> 42.
projection(_, X) -> X.
'''
user = '''-module(user).
-export([run/0, local/0]).
-type remote_chain() :: owner:chain(integer()).
-spec run() -> integer().
run() -> owner:id(owner:value()).
local() -> id(7).
id(X) -> X.
'''
(work / 'owner å.erl').write_text(owner, encoding='utf-8')
(work / 'user.erl').write_text(user, encoding='utf-8')


def run(*args, expected=0):
    result = subprocess.run([tool, *args], cwd=work, capture_output=True, text=True, encoding='utf-8', timeout=30)
    assert result.returncode == expected, (args, result.returncode, result.stdout, result.stderr)
    return result


args = ('--print-types', 'user.erl', 'owner å.erl')
result = run(*args)
assert result.stdout == run(*args).stdout
assert 'warning:' in result.stderr and 'contradicts specification' in result.stderr
text = result.stdout
assert text.index('module "user"') < text.index('module "owner"')
assert 'declared type "chain"/1 exported=true' in text
assert 'declared opaque "secret"/0' in text and 'declared nominal "nominal_id"/0' in text
assert 'declared callback "cb"/1 optional=true' in text
assert 'owner:chain(erlang:integer())' in text
assert re.search(r'function "run"/0[^\n]*declared=spec[^\n]*result=42', text)
assert re.search(r'function "local"/0[^\n]*declared=none[^\n]*result=7', text)
assert re.search(r'function "id"/1[^\n]*term\(\) \[unknown\][^\n]*argument\[0\]', text)
assert re.search(r'function "projection"/2[^\n]*argument\[1\]', text)
assert re.search(r'expression "user.erl":5:\d+ inferred=42', text)
assert 'target datalayout' not in text and 'define i' not in text
verbose = run('--verbose', *args)
assert verbose.stdout == text and 'phase=inference' in verbose.stderr
assert not re.search(r'phase=(lowering|specialization|verification|optimization|emission)', verbose.stderr)
assert not (work / 'build').exists()

# Each target gets independent facts and deterministic selected-target order.
(work / 'shared.erl').write_text('-module(shared). -export([value/0]). value() -> ?VALUE.\n', encoding='utf-8')
(work / 'project.toml').write_text('''schema_version=1
[[targets]]
name="one"
sources=["shared.erl"]
output="reserved"
[targets.options]
defines=["VALUE=1"]
[[targets]]
name="two"
sources=["shared.erl"]
output="reserved"
[targets.options]
defines=["VALUE=2"]
''', encoding='utf-8')
project = run('--print-types', '--project', 'project.toml', '--target', 'two', '--target', 'one', '--verbose')
assert project.stdout.index('target="two"') < project.stdout.index('target="one"')
assert re.search(r'target="two"[\s\S]*result=2[\s\S]*target="one"[\s\S]*result=1', project.stdout)
assert 'phase=lowering' not in project.stderr
assert not (work / 'reserved').exists() and not (work / 'build').exists()

for conflict in (('--print-ir',), ('--print-optimized-ir',), ('--emit', 'obj'), ('--artifact-dir', 'out'),
                 ('--output', 'exe'), ('-O0',), ('-O2',), ('--no-type-specialization',),
                 ('--target-triple', 'invalid'), ('--parse-check',), ('--preprocess-check',),
                 ('--print-pp',), ('--print-ast',)):
    rejected = run('--print-types', *conflict, 'missing.erl', expected=2)
    assert not rejected.stdout
assert not run('--print-types', '--new-project', 'new', expected=2).stdout
assert 'Usage:' in run('--help', '--print-types', 'missing.erl').stdout
assert not run('--print-types', 'user.erl', expected=1).stdout
(work / 'bad.erl').write_text('-module(bad). -type invalid() :: missing(). value() -> 1.\n', encoding='utf-8')
assert not run('--print-types', 'bad.erl', expected=1).stdout
(work / 'bad.erl').write_text('-module(bad). value( -> 1.\n', encoding='utf-8')
assert not run('--print-types', 'bad.erl', expected=1).stdout
assert not (work / 'new.toml').exists() and not (work / 'out').exists()

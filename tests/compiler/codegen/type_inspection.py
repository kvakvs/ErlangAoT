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

# Case and if results join like clause results; a name bound by several of their clauses stays unknown.
(work / 'branches.erl').write_text('''-module(branches).
-export([pick/1, same/1, mixed/1, shared/1, guarded/1, alike/1, bound/1]).
pick(X) -> case X of 1 -> 7; _ -> 7 end.
same(X) -> case X of {_} -> X; _ -> X end.
mixed(X) -> case X of 1 -> 1; _ -> 2 end.
shared(X) -> case X of 1 -> Y = 5; _ -> Y = 6 end, Y.
guarded(X) -> if X > 0 -> 1; true -> 2 end.
alike(X) -> if is_atom(X) -> X; true -> X end.
bound(X) -> if X > 0 -> Y = 5; true -> Y = 6 end, Y.
''', encoding='utf-8')
facts = run('--print-types', 'branches.erl').stdout
assert re.search(r'function "pick"/1[^\n]*result=7\n', facts), facts
assert re.search(r'function "same"/1[^\n]*argument\[0\]', facts), facts
assert re.search(r'function "mixed"/1[^\n]*result=union\(1, 2\)\n', facts), facts
assert re.search(r'function "shared"/1[^\n]*result=term\(\) \[unknown\]\n', facts), facts
assert re.search(r'function "guarded"/1[^\n]*result=union\(1, 2\)\n', facts), facts
assert re.search(r'function "alike"/1[^\n]*argument\[0\]', facts), facts
assert re.search(r'function "bound"/1[^\n]*result=term\(\) \[unknown\]\n', facts), facts

# Recursive components iterate from none() to a fixed point; pending recursive calls add nothing to a join.
(work / 'recursive.erl').write_text('''-module(recursive).
-export([zero/1, keep/2, swap/2, forever/0, even/1, odd/1, fact/1, outer/1]).
zero(0) -> 0; zero(N) -> zero(N - 1).
keep(Acc, 0) -> Acc; keep(Acc, N) -> keep(Acc, N - 1).
swap(A, 0) -> A; swap(A, B) -> swap(B, A).
forever() -> forever().
even(0) -> 1; even(N) -> odd(N - 1).
odd(0) -> 0; odd(N) -> even(N - 1).
fact(0) -> 1; fact(N) -> N * fact(N - 1).
outer(X) -> case X of 0 -> 5; _ -> zero(X) end.
''', encoding='utf-8')
facts = run('--print-types', 'recursive.erl').stdout
assert 'inferred=complete' in facts, facts
assert re.search(r'function "zero"/1[^\n]*result=0\n', facts), facts
assert re.search(r'function "keep"/2[^\n]*result=[^\n]*\[argument\[0\] relation\]\n', facts), facts
assert re.search(r'function "swap"/2[^\n]*result=term\(\) \[unknown\]\n', facts), facts
assert re.search(r'function "forever"/0[^\n]*result=none\(\)\n', facts), facts
assert re.search(r'function "even"/1[^\n]*result=union\(0, 1\)\n', facts), facts
assert re.search(r'function "odd"/1[^\n]*result=union\(0, 1\)\n', facts), facts
assert re.search(r'function "fact"/1[^\n]*result=term\(\) \[unknown\]\n', facts), facts
assert re.search(r'function "outer"/1[^\n]*result=union\(0, 5\)\n', facts), facts
# Final expression facts use the converged summaries: the recursive call inside even/1 sees odd's result.
assert re.search(r'expression "recursive.erl":7:\d+ inferred=union\(0, 1\)\n', facts), facts


# A cycle of n functions gains one result member per round: 15 converge, 16 hit the round limit and widen.
def ring(size):
    name = f'ring{size}'
    lines = [f'-module({name}).', '-export([w1/1]).']
    lines += [f'w{i}(X) -> case X of 0 -> {i}; _ -> w{i % size + 1}(X) end.' for i in range(1, size + 1)]
    (work / f'{name}.erl').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return run('--print-types', f'{name}.erl').stdout


converged, widened = ring(15), ring(16)
assert 'inferred=complete' in converged and 'union(1, 2, 3, ' in converged and ', 15)\n' in converged, converged
assert 'inferred=widened' in widened, widened
assert re.search(r'function "w1"/1[^\n]*result=term\(\) \[unknown\]\n', widened), widened

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

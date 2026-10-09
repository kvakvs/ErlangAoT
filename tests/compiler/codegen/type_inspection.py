"""--print-types: each module printed as source with its declarations, inferred function signatures and
`Expression :: Type` annotations, without entering LLVM lowering."""
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
-spec value() -> integer().
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
assert result.stderr == '', result.stderr
text = result.stdout
assert text.index('%% module "user"') < text.index('%% module "owner"')
for declaration in ('-export_type([chain/1]).', '-type chain(T) :: nil | {T, chain(T)}.',
                    '-opaque secret() :: integer().', '-nominal nominal_id() :: integer().',
                    '-spec id(T) -> T when T :: term().', '-callback cb(integer()) -> integer().',
                    '-optional_callbacks([cb/1]).', '-type remote_chain() :: owner:chain(integer()).'):
    assert declaration in text, declaration
assert '-spec run() -> integer().\n%% declared: run() -> integer()\n%% inferred: run() -> 42\nrun() ->\n' in text
assert '%% declared: id(T) -> T when T :: _\n%% inferred: id(X) -> X\n' in text
assert '%% declared: value() -> integer()\n%% inferred: value() -> 42\n' in text
assert '%% inferred: local() -> 7\nlocal() ->\n    id(7) :: 7.\n' in text
assert '%% inferred: id(X) -> X\n' in text
assert '%% inferred: projection(_, X) -> X\n' in text
assert '    owner:id((owner:value() :: 42)) :: 42.\n' in text
assert 'target datalayout' not in text and 'define i' not in text
verbose = run('--verbose', *args)
assert verbose.stdout == text and 'phase=inference' in verbose.stderr
assert not re.search(r'phase=(lowering|specialization|verification|optimization|emission)', verbose.stderr)
assert not (work / 'build').exists()

# A specification that shares no value with the inferred result is an error naming both types.
(work / 'contradiction.erl').write_text('-module(contradiction).\n-export([value/0]).\n-spec value() -> atom().\n'
                                       'value() -> 42.\n', encoding='utf-8')
contradiction = run('--print-types', 'contradiction.erl', expected=1)
assert ('contradiction.erl:3:1: inferred result contradicts specification for value: declared atom(), inferred 42'
        in contradiction.stderr), contradiction.stderr

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
for signature in ('pick(_) -> 7', 'same(X) -> X', 'mixed(_) -> 1..2',
                  'shared(_) -> _', 'guarded(_) -> 1..2', 'alike(X) -> X',
                  'bound(_) -> _'):
    assert f'%% inferred: {signature}\n' in facts, (signature, facts)
# The case value joins its clauses, though the name its clauses bind stays unknown.
assert '    end :: 5..6,\n    Y.\n' in facts, facts

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
# Inputs are success domains: N - 1 makes N a number() in every clause that returns.
assert '%% inferred: keep(Acc, number()) -> Acc\n' in facts, facts
for signature in ('zero(number()) -> 0', 'swap(_, _) -> _', 'forever() -> none()',
                  'even(number()) -> 0..1', 'odd(number()) -> 0..1', 'fact(number()) -> number()',
                  'outer(number()) -> 0 | 5'):
    assert f'%% inferred: {signature}\n' in facts, (signature, facts)
# Final expression facts use the converged summaries: the recursive call inside even/1 sees odd's result.
assert '    odd((N - 1 :: number())) :: 0..1.\n' in facts, facts


# A cycle of n functions gains one result member per round (docs/semantic.md#inference-domain): a cycle of 8, the
# join rounds, converges exactly; a longer one widens to the integers' category and still converges.
def ring(size):
    name = f'ring{size}'
    lines = [f'-module({name}).', '-export([w1/1]).']
    lines += [f'w{i}(X) -> case X of 0 -> {i}; _ -> w{i % size + 1}(X) end.' for i in range(1, size + 1)]
    (work / f'{name}.erl').write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return run('--print-types', f'{name}.erl').stdout


exact, widened = ring(8), ring(16)
assert 'inferred=complete' in exact and '%% inferred: w1(_) -> 1..8\n' in exact, exact
assert 'inferred=complete' in widened, widened
assert '%% inferred: w1(_) -> pos_integer()\n' in widened, widened

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
assert re.search(r'target="two"[\s\S]*value\(\) -> 2\n[\s\S]*target="one"[\s\S]*value\(\) -> 1\n',
                 project.stdout)
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

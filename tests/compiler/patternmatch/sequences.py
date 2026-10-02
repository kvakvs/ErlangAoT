"""Compare body sequences, chained/compound matches and structured failures with OTP."""
import itertools
import json
import pathlib
import re
import sys
from evidence import digest, provenance, run
from immediate import native
from services import VALUES, write_calls


def kernels(otp, work):
    """Retain match_SUITE assignment order while replacing unavailable tuple construction explicitly."""
    path = otp / 'lib/compiler/test/match_SUITE.erl'
    text = path.read_text(encoding='utf-8')
    mutable = re.search(r'^mutable_variables_1\(\) ->.*?\n    \{Result,One,Zero\}\.', text, re.M | re.S).group()
    definitions = [
        ('mutable_variables_1', 0, mutable.replace('{Result,One,Zero}.', 'Result.')),
        ('id', 1, 'id(I) -> I.'),
        ('bind', 1, 'bind(X) -> Y = X, Y.'),
        ('check_only', 1, 'check_only(X) -> X = 1.'),
        ('self', 1, 'self(X) -> X = X.'),
        ('rebind', 2, 'rebind(X,Y) -> X = Y, X.'),
        ('chain', 1, 'chain(X) -> A = B = C = X, A = B, C.'),
        ('chain_check', 2, 'chain_check(X,Y) -> Z = X = Y, Z.'),
        ('compound', 1, 'compound(X) -> (A = B) = X, A = B, B.'),
        ('compound_check', 2, 'compound_check(X,Y) -> (A = X) = Y, A.'),
        ('rhs_binding', 1, 'rhs_binding(X) -> Y = (Y = X), Y.'),
        ('rhs_old', 1, 'rhs_old(X) -> X = (Y = X), Y.'),
        ('wild', 1, 'wild(X) -> _ = X, _ = ok, _Name = X, _Name.'),
        ('literal', 1, 'literal(X) -> pattern_atom = X, ok.'),
        ('constant', 1, 'constant(X) -> (1+2) = X, X.'),
        ('empty_list', 1, 'empty_list(X) -> "" = X, [].'),
        ('empty_tuple', 1, 'empty_tuple(X) -> {} = X, X.'),
        ('stop', 1, 'stop(X) -> ok = X, hd([]).'),
        ('body_failure', 1, 'body_failure(X) when is_integer(X) -> ok = X, later; body_failure(_) -> fallback.'),
        ('later_call', 1, 'later_call(X) -> first, Y = id(X), id(Y).'),
        ('in_call', 1, 'in_call(X) -> id(A = B = X), A = B, A.'),
        ('sibling', 2, 'sibling(X,Y) -> first(A = X, A = Y), A.'),
        ('first', 2, 'first(X,_) -> X.'),
        ('chained_failure', 0, 'chained_failure() -> A = 1 = 2 = 3, A.'),
        ('early_call_error', 1, 'early_call_error(X) -> hd(X), ok = impossible.'),
        ('wrong_spec', 1, '-spec wrong_spec(integer()) -> integer().\nwrong_spec(X) -> Y = X, Y.'),
        ('wide', 1, 'wide(X) -> ' + ', '.join(f'V{i} = X' for i in range(128)) + ', V127.'),
        ('deep', 1, 'deep(X) -> ' + ' = '.join(f'V{i}' for i in range(128)) + ' = X, V0.')]
    exports = ','.join(f'{name}/{arity}' for name, arity, _ in definitions)
    (work / 'answer.erl').write_text(text.split('-module(')[0] + f'-module(answer).\n-export([{exports}]).\n' +
        '\n'.join(body for _, _, body in definitions) + '\n', encoding='utf-8')
    (work / 'client.erl').write_text('-module(client).\n-export([nested/1,stop/1,retry/1]).\n'
        'nested(X) -> answer:stop(answer:bind(X)), answer:id(unreachable).\n'
        'stop(X) -> answer:body_failure(X).\nretry(X) -> Y = answer:bind(X), Y.\n', encoding='utf-8')
    values = VALUES + ['ok', 'pattern_atom', 1, 2, 3]
    calls = [('answer', name, [value]) for name, arity, _ in definitions if arity == 1 for value in values]
    calls += [('answer', name, list(args)) for name, arity, _ in definitions if arity == 2
              for args in itertools.product(values, repeat=2)]
    calls += [('answer', name, []) for name, arity, _ in definitions if arity == 0]
    calls += [('client', name, [value]) for name in ['nested', 'stop', 'retry'] for value in values]
    write_calls(work, calls)
    return {'calls': len(calls), 'source': str(path.relative_to(otp)), 'sha256': digest(path),
            'function': 'mutable_variables_1/0; match_in_call/1 and mac_c/1 adaptations; id/1',
            'original_mutable_helper': mutable,
            'adaptations': 'mutable_variables_1 retains all assignments and guaranteed failure before replacing the unreachable tuple with Result; in_call adapts mac_c chain binding inside id calls using immediate X in place of constructed tuples; authored kernels test chains, aliases, equality constraints, RHS-created bindings, errors and sequences without removing tested matching behavior; id/1 unchanged',
            'declarations': exports, 'wrapper_sha256': digest(work / 'answer.erl'), 'client_sha256': digest(work / 'client.erl')}


def main():
    tool, cmake, root, otp_root, directory, settings, config, suffix, escript = sys.argv[1:]
    source, otp, work = pathlib.Path(root), pathlib.Path(otp_root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    provenance(source, otp, source / 'tests/fixtures/patternmatch', work)
    records = kernels(otp, work)
    expected = run([escript, str(source / 'tests/compiler/patternmatch/immediate.escript'), str(work)])
    (work / 'expected.txt').write_bytes(expected.replace('\r\n', '\n').encode())
    native(tool, cmake, source, work, settings, config, suffix)
    for mode in ['--print-types', '--print-ir', '--print-optimized-ir']:
        options = [] if mode == '--print-types' else ['-O2']
        run([tool, *options, mode, str(work / 'answer.erl'), str(work / 'client.erl')])
    (work / 'evidence.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
    print(f'{records["calls"]} OTP/native sequence and match calls passed in four policies.')


if __name__ == '__main__':
    main()

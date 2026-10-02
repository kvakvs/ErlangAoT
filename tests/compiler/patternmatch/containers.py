"""Execute complete tuple/list helpers and checked construction/matching against OTP."""
import itertools
import json
import pathlib
import re
import sys
from evidence import digest, provenance, run
from immediate import native
from services import write_calls


def kernels(otp, work):
    """Keep complete selected helpers, their upstream notices and explicit adaptation records."""
    suites = {name: (otp / f'lib/compiler/test/{name}.erl').read_text(encoding='utf-8')
              for name in ['match_SUITE', 'bif_SUITE', 'beam_type_SUITE']}
    definitions, records = [], []
    names = ['str_alias_1', 'tuple_alias_a', 'tuple_alias_b', 'list_in_tuple_a', 'list_in_tuple_b',
             'tuple_in_tuple_a', 'tuple_in_tuple_b', 'multiple_aliases_1a', 'multiple_aliases_1b',
             'multiple_aliases_2', 'multiple_aliases_3a', 'multiple_aliases_3b',
             'multiple_aliases_4a', 'multiple_aliases_4b', 'list_alias1a', 'list_alias1b',
             'list_alias2a', 'list_alias2b', 'list_alias3a', 'list_alias3b']
    for suite, name, arity in [('match_SUITE', n, 1) for n in names] + [
            ('bif_SUITE', 'first', 2), ('match_SUITE', 'id', 1),
            ('beam_type_SUITE', 'do_tuple', 0), ('beam_type_SUITE', 'do_literal_tuple_1', 1),
            ('beam_type_SUITE', 'do_literal_tuple_2', 1)]:
        body = re.search(r'^' + name + r'\(.*?\.(?=\n)', suites[suite], re.M | re.S).group()
        definitions.append((name, arity, body))
        records.append({'suite': suite, 'function': f'{name}/{arity}', 'source': body, 'adaptations': 'none'})
    definitions += [
        ('make', 1, 'make(X) -> {id(X), [X,{X},"Ω"|X], "abc"}.'),
        ('head_tail', 1, 'head_tail(_) -> 1 = hd(id([1])), [] = tl(id([1])), ok.'),
        ('prefix', 1, 'prefix("ab" ++ T) -> T; prefix(_) -> no.'),
        ('nested_prefix', 1, 'nested_prefix([97|[98|[]]] ++ T) -> T; nested_prefix(_) -> no.'),
        ('empty_prefix', 1, 'empty_prefix([] ++ T) -> T.'),
        ('repeat', 2, 'repeat(X,X) -> equal; repeat(_,_) -> different.'),
        ('independent', 1, 'independent(X) -> A = {X,[X]}, B = {X,[X]}, A = B, A =:= B.'),
        ('extract', 1, 'extract({_,[A|T]}) -> id({A,T}); extract(X) -> X.'),
        ('body', 1, 'body(X) -> {A,[B|T]} = X, {A,B,T}.'),
        ('fallthrough', 1, 'fallthrough({A,A}) -> A; fallthrough([A,A]) -> A; fallthrough({_,A}) -> A; fallthrough(X) -> X.'),
        ('badmatch', 1, 'badmatch(X) -> {impossible} = {X,[X]}, unreachable.'),
        ('order', 2, 'order(X,Y) -> {X =:= Y, X == Y, X < Y, X =< Y, X > Y, X >= Y, min(X,Y), max(X,Y)}.'),
        ('queries', 1, 'queries(X) -> {is_tuple(X), is_list(X), is_atom(X), is_integer(X)}.'),
        ('guard', 1, 'guard(X) when length(X) =:= 2 -> two; guard(X) when tuple_size(X) =:= 2 -> pair; guard(_) -> other.'),
        ('guard_construct', 1, 'guard_construct(X) when element(2,{a,X}) =:= hd([X]) -> X.'),
        ('head', 1, 'head(X) -> hd(X).'), ('tail', 1, 'tail(X) -> tl(X).'),
        ('length_value', 1, 'length_value(X) -> length(X).'),
        ('size_value', 1, 'size_value(X) -> size(X).'),
        ('element_value', 2, 'element_value(N,X) -> element(N,X).'),
        ('wrong_spec', 1, '-spec wrong_spec(integer()) -> integer().\nwrong_spec({_,X}) -> X; wrong_spec([X|_]) -> X; wrong_spec(X) -> X.'),
        ('source_order', 1, 'source_order(X) -> T = {A = id(X), B = id(X)}, {T,A,B}.'),
        ('failure_order', 1, 'failure_order(X) -> {hd(X), ok = impossible}.'),
        ('wide', 1, 'wide(X) -> {' + ','.join(['X'] * 128) + '}.')]
    exports = ','.join(f'{name}/{arity}' for name, arity, _ in definitions)
    notices = '\n'.join(text.split('-module(')[0] for text in suites.values())
    (work / 'answer.erl').write_text(notices +
        f'-module(answer).\n-export([{exports}]).\n' + '\n'.join(body for _, _, body in definitions) + '\n', encoding='utf-8')
    (work / 'client.erl').write_text('-module(client).\n-export([nested/1,retain/1,fail/1]).\n'
        'nested(X) -> answer:id(answer:first(X, answer:make(X))).\n'
        'retain(X) -> A = answer:extract(X), answer:make(A), answer:id(A).\n'
        'fail(X) -> answer:badmatch(X).\n', encoding='utf-8')
    values = [0, 1, 'ok', 'true', [], (), [1], [1,2], ['a','b'], [1,1], [1,2,3],
              {'cons': [1,'tail']}, {'cons': [97,{'cons': [98,'tail']} ]},
              (1,), (1,2), (1,1), (1,2,3), ('container',[1,2,3],4),
              ('x',('y',[1]),[2]), ([],[[1],[2]]), ([1], [1]), (('a',),('a',))]
    values += [list(map(ord, s)) for s in ['', 'a', 'ab', 'abc', 'def', 'ghi', 'klm', 'qrs', 'xy', 'Ω']]
    calls = [('answer', name, [value]) for name, arity, _ in definitions if arity == 1 for value in values]
    calls += [('answer', name, list(pair)) for name in ['repeat','order','first'] for pair in itertools.product(values, repeat=2)]
    calls += [('answer', 'element_value', [n,value]) for n in [-1,0,1,2,20,21,'ok'] for value in values]
    calls += [('answer', name, []) for name, arity, _ in definitions if arity == 0]
    calls += [('client', name, [value]) for name in ['nested','retain','fail'] for value in values]
    write_calls(work, calls)
    return {'calls': len(calls), 'helpers': records, 'declarations': exports,
            'adaptations': 'head_tail/1 retains hd/tl assertions with id/1; case-dispatch helpers are covered by equivalent ordered guard clauses in guard/1; remaining authored kernels retain all indicated behavior',
            'sources': {f'lib/compiler/test/{n}.erl': digest(otp / f'lib/compiler/test/{n}.erl') for n in suites},
            'wrapper_sha256': digest(work / 'answer.erl'), 'client_sha256': digest(work / 'client.erl')}


def main():
    tool, cmake, root, otp_root, directory, settings, config, suffix, escript = sys.argv[1:]
    source, otp, work = pathlib.Path(root), pathlib.Path(otp_root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    provenance(source, otp, source / 'tests/fixtures/patternmatch', work)
    records = kernels(otp, work)
    expected = run([escript, str(source / 'tests/compiler/patternmatch/immediate.escript'), str(work)])
    (work / 'expected.txt').write_bytes(expected.replace('\r\n', '\n').encode())
    native(tool, cmake, source, work, settings, config, suffix)
    ir = run([tool, '--print-ir', str(work / 'answer.erl'), str(work / 'client.erl')])
    assert 'inspect.outcome' in ir and 'container.outcome' in ir
    assert re.search(r'container.arguments\w* = getelementptr i64, ptr %roots', ir)
    for triple in ['i686-pc-windows-msvc', 'x86_64-unknown-linux-gnu', 'i686-unknown-linux-gnu',
                   'aarch64-unknown-linux-gnu', 'armv7-unknown-linux-gnueabihf',
                   'aarch64-apple-darwin', 'aarch64-pc-windows-msvc']:
        run([tool, '--target-triple', triple, '--emit', 'obj', '--artifact-dir', str(work / triple), str(work / 'answer.erl'), str(work / 'client.erl')])
    (work / 'evidence.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
    print(f'{records["calls"]} OTP/native container calls passed in four policies; seven foreign object targets.')


if __name__ == '__main__':
    main()

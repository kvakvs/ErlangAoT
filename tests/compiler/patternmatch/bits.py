"""Exercise packed bit construction, checked cursors and retained tails using project-owned OTP goldens."""
import itertools
import json
import pathlib
import re
import sys
from evidence import digest, run
from immediate import native
from services import write_calls
from stored import load


def bits(data, length=None):
    """Supply logical packed bits independently of runtime representation and padding."""
    return {'bits': bytes(data).hex(), 'length': len(data) * 8 if length is None else length}


def kernels(otp, work):
    """Retain complete tail helpers and explicitly label suite adaptations outside the Common Test harness."""
    suites = ['bs_construct_SUITE', 'bs_match_SUITE', 'bs_size_expr_SUITE', 'bs_bit_binaries_SUITE', 'bs_utf_SUITE']
    paths = [otp / ('lib/compiler/test/' + name + '.erl') for name in suites]
    source = paths[1].read_text(encoding='utf8')
    definitions, helpers = [], []
    for name in ['bin_tail_c', 'bin_tail_c_dead', 'bin_tail_c_var', 'bin_tail_d_dead', 'bin_tail_d_var']:
        body = re.search(r'^' + name + r'\(.*?\.(?=\n)', source, re.M | re.S).group()
        definitions.append((name, 2, body))
        helpers.append({'source': paths[1].relative_to(otp).as_posix(), 'source_sha256': digest(paths[1]),
                        'function': name + '/2', 'body': body, 'adaptations': 'none'})
    definitions += [
        ('build', 2, 'build(X,N) -> <<X:N>>.'),
        ('little', 2, 'little(X,N) -> <<X:N/little>>.'),
        ('native', 2, 'native(X,N) -> <<X:N/native>>.'),
        ('unit', 2, 'unit(X,N) -> <<X:N/signed-little-unit:7>>.'),
        ('unit256', 1, 'unit256(X) -> <<X:1/unit:256>>.'),
        ('read_unit256', 1, 'read_unit256(<<X:1/unit:256,T/bits>>) -> {X,T}; read_unit256(_) -> no.'),
        ('bytes', 1, 'bytes(B) -> <<B/bytes>>.'),
        ('clone', 1, 'clone(B) -> <<B/bits>>.'),
        ('join', 2, 'join(A,B) -> <<A/bitstring,B/bitstring>>.'),
        ('prefix', 2, 'prefix(X,B) -> <<X:3,B/bitstring,X:5>>.'),
        ('all_explicit', 1, 'all_explicit(B) -> <<B:all/binary>>.'),
        ('all_pattern', 1, 'all_pattern(<<T:all/binary>>) -> T; all_pattern(_) -> no.'),
        ('float_literal', 1, 'float_literal(<<1:16/float,T/bitstring>>) -> {integer,T}; float_literal(<<1.00146484375:16/float,T/bitstring>>) -> {rounded,T}; float_literal(<<-0.0:32/float,T/bitstring>>) -> {negative,T}; float_literal(_) -> no.'),
        ('map_key', 2, 'map_key(K,B) -> #{K := V} = #{B => B}, V.'),
        ('float_be', 2, 'float_be(X,N) -> <<X:N/float>>.'),
        ('float_le', 2, 'float_le(X,N) -> <<X:N/float-little>>.'),
        ('utf8', 1, 'utf8(X) -> <<X/utf8>>.'),
        ('utf16', 1, 'utf16(X) -> <<X/utf16>>.'),
        ('utf16le', 1, 'utf16le(X) -> <<X/utf16-little>>.'),
        ('utf32', 1, 'utf32(X) -> <<X/utf32-native>>.'),
        ('literal', 0, 'literal() -> <<"abc":16,"Ω𐀀"/utf8,0:3>>.'),
        ('empty_string', 1, 'empty_string(N) -> <<"":N,5:3>>.'),
        ('strings', 1, 'strings(N) -> <<"abc":N>>.'),
        ('head', 1, 'head(<<A:3,B:5,T/bitstring>>) -> {A,B,T}; head(_) -> no.'),
        ('zero', 1, 'zero(<<42>>) -> star; zero(<<V:0>>) -> V; zero(_) -> no_match.'),
        ('overfull', 1, 'overfull(<<256:8>>) -> impossible; overfull(_) -> no.'),
        ('dependent', 1, 'dependent(<<N:8,T:N/binary,R/bitstring>>) -> {N,T,R}; dependent(_) -> no.'),
        ('repeat', 1, 'repeat(<<X:8,X:8,T/bitstring>>) -> {X,T}; repeat(_) -> no.'),
        ('bound', 3, 'bound(N,X,B) -> <<X:N,T/bitstring>> = B, T.'),
        ('read', 2, 'read(N,B) -> <<X:N,T/bitstring>> = B, {X,T}.'),
        ('read_le', 2, 'read_le(N,B) -> <<X:N/little,T/bitstring>> = B, {X,T}.'),
        ('read_signed', 2, 'read_signed(N,B) -> <<X:N/signed,T/bitstring>> = B, {X,T}.'),
        ('read_signed_le', 2, 'read_signed_le(N,B) -> <<X:N/signed-little,T/bitstring>> = B, {X,T}.'),
        ('read_native', 2, 'read_native(N,B) -> <<X:N/native,T/bitstring>> = B, {X,T}.'),
        ('read_float', 2, 'read_float(N,B) -> <<X:N/float,T/bitstring>> = B, {X,T}.'),
        ('read_float_le', 2, 'read_float_le(N,B) -> <<X:N/float-little,T/bitstring>> = B, {X,T}.'),
        ('get_utf8', 1, 'get_utf8(<<X/utf8,T/bitstring>>) -> {X,T}; get_utf8(_) -> no.'),
        ('get_utf16', 1, 'get_utf16(<<X/utf16,T/bitstring>>) -> {X,T}; get_utf16(_) -> no.'),
        ('get_utf16le', 1, 'get_utf16le(<<X/utf16-little,T/bitstring>>) -> {X,T}; get_utf16le(_) -> no.'),
        ('get_utf32', 1, 'get_utf32(<<X/utf32-native,T/bitstring>>) -> {X,T}; get_utf32(_) -> no.'),
        ('literal_head', 1, 'literal_head(<<"abc",T/bitstring>>) -> T; literal_head(_) -> no.'),
        ('utf_literal', 1, 'utf_literal(<<"Ω𐀀"/utf8,T/bitstring>>) -> T; utf_literal(_) -> no.'),
        ('binary_tail', 1, 'binary_tail(<<_:1,T/binary>>) -> T; binary_tail(_) -> no.'),
        ('all_unit', 1, 'all_unit(<<T/binary-unit:3>>) -> T; all_unit(_) -> no.'),
        ('body', 1, 'body(B) -> <<1:3,T/bitstring>> = B, T.'),
        ('shared', 1, 'shared(<<_,T/bitstring>>) -> <<_,R/bitstring>> = T, {T,R}; shared(_) -> no.'),
        ('classify', 1, 'classify(B) -> {is_binary(B),is_bitstring(B),is_list(B),is_tuple(B)}.'),
        ('sizes', 1, 'sizes(B) -> {size(B),byte_size(B),bit_size(B)}.'),
        ('part', 3, 'part(B,S,N) -> binary_part(B,S,N).'),
        ('part_tuple', 2, 'part_tuple(B,P) -> erlang:binary_part(B,P).'),
        ('guard_part', 3, 'guard_part(B,S,N) when byte_size(binary_part(B,S,N)) >= 0 -> yes; guard_part(_,_,_) -> no.'),
        ('guard_build', 2, 'guard_build(X,N) when bit_size(<<X:N>>) > 0 -> yes; guard_build(_,_) -> no.'),
        ('size_failure', 1, 'size_failure(<<X:(1 div 0)>>) -> X; size_failure(_) -> recovered.'),
        ('guard_alternative', 1, 'guard_alternative(X) when bit_size(<<X/utf8>>) > 0; is_atom(X) -> yes; guard_alternative(_) -> no.'),
        ('compare', 2, 'compare(A,B) -> {A =:= B,A == B,A < B,A =< B,A > B,A >= B}.'),
        ('alias', 1, 'alias(<<1,T/bitstring>> = B) -> {B,T}; alias(_) -> no.'),
        ('duplicate', 2, 'duplicate(B,B) -> same; duplicate(_,_) -> different.'),
        ('wrong_spec', 1, '-spec wrong_spec(integer()) -> integer().\nwrong_spec(<<X,T/binary>>) -> {X,T}; wrong_spec(_) -> no.'),
        ('ordered', 1, 'ordered(X) -> <<X:0,(1 div 0)>>.'),
        ('id', 1, 'id(X) -> X.')]
    exports = ','.join(f'{name}/{arity}' for name, arity, _ in definitions)
    (work / 'answer.erl').write_bytes((source.split('-module(')[0] + f'-module(answer).\n-export([{exports}]).\n' +
        '\n'.join(body for _, _, body in definitions) + '\n').encode())
    (work / 'client.erl').write_bytes(b'-module(client).\n-export([retain/1,nested/2]).\n'
        b'retain(B) -> {T,R} = answer:shared(B), answer:literal(), {answer:id(T),answer:id(R)}.\n'
        b'nested(X,N) -> answer:read(N,answer:little(X,N)).\n')
    (work / 'project.toml').write_bytes(b'schema_version=1\n[[targets]]\nname="bits"\nsources=["answer.erl","client.erl"]\n')
    binaries = [bits([]), bits([0]), bits([1]), bits([42]), bits([1,1]), bits([1,2]), bits([255,255]),
        bits([2,104,105]), bits(b'abcde'), bits([0x80],1), bits([0x20],3), bits([1,0x80],9),
        bits(list(range(80))), bits([0xf0,0x90,0x80,0x80]), bits([0xc0,0x80]), bits([0xed,0xa0,0x80]),
        bits([0xf4,0x90,0x80,0x80]), bits([0xe2,0x82]), bits([0xd8,0,0xdc,0]), bits([0xdc,0]),
        bits([0,0x3e]), bits([0x3e,0]), bits([0x3c,0]), bits([0x3c,2]), bits([0x54,0x10]), bits([0,0,0,0]), bits([0x80,0,0,0]), bits([0x7c,0]), bits([0x7f,0xc0,0,0]), bits([0,0,0x80,0x3f]),
        bits([0x3f,0x80,0,0]), bits([0,0,0,0,0,0,0xf0,0x3f]), bits('Ω𐀀'.encode())]
    ordinary = [0,-1,1.0,'a',[],(),(1,),{'map':[('a',1)]}]
    values = binaries + ordinary
    integers = [0,1,-1,127,255,256,-257,2**100,2**100+3]
    sizes = [-1,0,1,3,7,8,9,12,16,31,32,64,100,128,1.0,'a']
    calls = [('answer',name,[value]) for name,arity,_ in definitions if arity == 1 for value in values]
    calls += [('answer',name,[x,n]) for name in ['build','little','native','unit','guard_build']
              for x in integers + [1.0,'a'] for n in sizes]
    calls += [('answer',name,[n,b]) for name in ['read','read_le','read_native','read_signed','read_signed_le','read_float','read_float_le']
              for n in sizes for b in binaries]
    floats = [-0.0,0.0,1,1.5,-2.5,65504.0,65520.0,2**-24,2**-25,1.00048828125,1.00146484375,1.0e100,'a']
    calls += [('answer',name,[x,n]) for name in ['float_be','float_le'] for x in floats for n in [0,1,16,32,64,128]]
    unicode = [-1,0,127,128,2047,2048,0xd7ff,0xd800,0xdfff,0xe000,0x10000,0x10ffff,0x110000,1.0,'a']
    calls += [('answer',name,[value]) for name in ['utf8','utf16','utf16le','utf32'] for value in unicode]
    calls += [('answer',name,[a,b]) for name in ['compare','duplicate','join'] for a,b in itertools.product(values[:15],repeat=2)]
    calls += [('answer','map_key',[a,b]) for a,b in itertools.product(binaries[:16],repeat=2)]
    calls += [('answer','prefix',[x,b]) for x in [1,255,'a'] for b in values]
    calls += [('answer','bound',[n,x,b]) for n in [0,3,8,'a'] for x in [0,1,'a'] for b in values[:12]]
    calls += [('answer',name,[b,s,n]) for name in ['part','guard_part'] for b in values for s,n in [(0,0),(0,1),(1,2),(3,-2),(-1,1),(100,0),('a',1),(0,1.0)]]
    calls += [('answer','part_tuple',[b,p]) for b in values for p in [(0,1),(3,-2),(0,),(),'a']]
    calls += [('answer',name,[b,n]) for name in ['bin_tail_c','bin_tail_c_var','bin_tail_d_dead','bin_tail_d_var']
              for b in binaries[:15] for n in [0,1,2,8,16,-1,'a']]
    calls += [('answer','literal',[])]
    calls += [('client','retain',[b]) for b in binaries]
    calls += [('client','nested',[x,n]) for x in integers for n in [0,3,8,12,128]]
    write_calls(work,calls)
    adaptations = [
        {'source': paths[0].relative_to(otp).as_posix(), 'function': 'strings/1',
         'kernels': ['strings/1','literal/0','empty_string/1'], 'changes': 'Parameterize segment inputs; preserve scalar/string construction and errors.'},
        {'source': paths[1].relative_to(otp).as_posix(), 'function': 'bad_size/1',
         'kernels': ['read/2','size_failure/1'], 'changes': 'Parameterize sizes and retain head/body rejection outcomes; omit Common Test catch harness.'},
        {'source': paths[1].relative_to(otp).as_posix(), 'function': 'zero_width/1',
         'kernels': ['zero/1','read_float/2','float_be/2'], 'changes': 'Parameterize zero-size fields and preserve integer/float construction versus extraction behavior.'},
        {'source': paths[1].relative_to(otp).as_posix(), 'function': 'shared_sub_bins/1',
         'kernels': ['shared/1','client:retain/1'], 'changes': 'Use two sequential tail extractions and remote retention; recursive traversal remains F21.'},
        {'source': paths[4].relative_to(otp).as_posix(), 'function': 'literals/1',
         'kernels': ['utf_literal/1','utf8/1','utf16/1','utf16le/1','utf32/1'], 'changes': 'Parameterize scalar/UTF literal inputs; omit unrelated comprehension and Common Test harness.'}]
    return {'calls':len(calls),'sources':{p.relative_to(otp).as_posix():digest(p) for p in paths},'helpers':helpers,
        'adaptation_records':adaptations,
        'adaptations':'strings/1, zero_width/1, bad_size/1 and UTF literals become parameterized complete kernels; shared_sub_bins retained-tail behavior uses two sequential extractions, with recursive traversal deferred to F21; no tested segment operation was removed',
        'wrapper_sha256':digest(work/'answer.erl'),'client_sha256':digest(work/'client.erl')}


def main():
    tool,cmake,root,directory,settings,config,suffix = sys.argv[1:]
    source,work = pathlib.Path(root),pathlib.Path(directory)
    records = load(source,'bits',work)
    native(tool,cmake,source,work,settings,config,suffix)
    ir = run([tool,'--print-ir',str(work/'answer.erl'),str(work/'client.erl')])
    assert 'binary.cursor' in ir and 'binary.outcome' in ir
    target_reports = []
    for triple in ['i686-pc-windows-msvc','x86_64-pc-windows-msvc','aarch64-unknown-linux-gnu']:
        run([tool,'--target-triple',triple,'--emit','obj','--artifact-dir',str(work/triple),str(work/'answer.erl'),str(work/'client.erl')])
        readobj = pathlib.Path(tool).parents[3] / 'thirdparty/clang+llvm-23.1.2-x86_64-pc-windows-msvc/bin/llvm-readobj.exe'
        if not readobj.exists():
            import shutil
            readobj = pathlib.Path(shutil.which('llvm-readobj') or '')
        objects = sorted(p for p in (work/triple).iterdir() if p.suffix in ['.obj','.o'])
        assert len(objects) == 2
        arch, width = {'i686-pc-windows-msvc':('i386','32bit'),
                       'x86_64-pc-windows-msvc':('x86_64','64bit'),
                       'aarch64-unknown-linux-gnu':('aarch64','64bit')}[triple]
        reports = [run([str(readobj),'--file-headers','--symbols',str(obj)]) for obj in objects]
        assert all('Arch: ' + arch in report and 'AddressSize: ' + width in report for report in reports)
        assert any('erlang_aot_bits_v1' in report for report in reports)
        target_reports.append({'triple':triple,'execution':'not attempted','header_and_symbols':'checked'})
    records['targets'] = target_reports
    run([tool,'--print-types',str(work/'answer.erl'),str(work/'client.erl')])
    (work/'evidence.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
    print(f'{records["calls"]} bitstring golden calls passed in four policies; foreign objects separately inspected.')


if __name__ == '__main__':
    main()

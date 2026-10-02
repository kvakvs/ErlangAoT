"""Execute immutable maps, exact keys and scoped key patterns against the pinned OTP oracle."""
import itertools
import json
import pathlib
import re
import sys
from evidence import digest, provenance, run
from immediate import native
from services import write_calls


def mapping(*entries):
    """Represent exact Erlang keys without Python's integer/float or signed-zero key coalescing."""
    return {'map':list(entries)}


def kernels(otp, work):
    """Retain complete size/get guards and label adaptations of updates, duplicate keys and computed keys."""
    path = otp/'lib/compiler/test/map_SUITE.erl'
    source = path.read_text(encoding='utf8')
    definitions,helpers = [],[]
    for name,arity in [('map_is_size',2),('check_map_value',3),('map_get_head',1)]:
        body = re.search(r'^'+name+r'\(.*?\.(?=\n)',source,re.M|re.S).group()
        definitions.append((name,arity,body))
        helpers.append({'function':name+'/'+str(arity),'body':body,'adaptations':'none'})
    definitions += [
        ('empty',0,'empty() -> #{}.'),
        ('literal',0,'literal() -> #{a => 1,b => 2,a => 3,1 => integer,1.0 => float,0.0 => positive,-0.0 => negative}.'),
        ('construct',2,'construct(K,V) -> #{K => V,{key,K} => {V,[V]},a => K}.'),
        ('associate',3,'associate(M,K,V) -> M#{K => V}.'),
        ('update',3,'update(M,K,V) -> M#{K := V}.'),
        ('mixed',3,'mixed(M,K,V) -> M#{K => first,K := V}.'),
        ('exact_first',3,'exact_first(M,K,V) -> M#{K := first,K => V}.'),
        ('get',2,'get(K,M) -> map_get(K,M).'),
        ('contains',2,'contains(K,M) -> is_map_key(K,M).'),
        ('size_value',1,'size_value(M) -> map_size(M).'),
        ('classify',1,'classify(M) -> {is_map(M),is_tuple(M),is_number(M)}.'),
        ('head',1,'head(#{a := V}) -> V; head(_) -> missing.'),
        ('empty_head',1,'empty_head(#{}) -> map; empty_head(_) -> other.'),
        ('extra',1,'extra(#{a := 1,b := V}) -> V; extra(_) -> no.'),
        ('duplicate',1,'duplicate(#{a := X,a := X}) -> X; duplicate(_) -> no.'),
        ('dup_keys',1,"dup_keys(#{'__struct__' := _} = M) -> dup_keys_inner(M); dup_keys(M) -> M."),
        ('dup_keys_inner',1,"dup_keys_inner(#{'__struct__' := _}) -> ok; dup_keys_inner(M) -> M."),
        ('contradictory',1,'contradictory(#{a := 1,a := 2}) -> impossible; contradictory(_) -> no.'),
        ('numeric_keys',1,'numeric_keys(#{1 := X,1.0 := Y}) -> {X,Y}; numeric_keys(_) -> no.'),
        ('nested',1,'nested(#{a := {X,[X|T]}} = M) -> {X,T,M}; nested(_) -> no.'),
        ('compound_key',2,'compound_key(K,M) -> #{{tag,K+1} := V} = M, V.'),
        ('bound_key',2,'bound_key(K,M) -> #{K := V} = M, V.'),
        ('key_call',3,'key_call(I,T,M) -> #{element(I,T) := V} = M, V.'),
        ('key_arithmetic',3,'key_arithmetic(A,B,M) -> #{A div B := V} = M, V.'),
        ('equal_keys',3,'equal_keys(A,B,M) -> #{A := X,B := X} = M, X.'),
        ('key_fail_head',1,'key_fail_head(#{1 div 0 := _}) -> impossible; key_fail_head(_) -> recovered.'),
        ('key_constant',1,'key_constant(#{(1+2) := V}) -> V; key_constant(_) -> no.'),
        ('body',1,'body(M) -> #{a := 1} = M, M.'),
        ('repeat',2,'repeat(M,M) -> same; repeat(_,_) -> different.'),
        ('compare',2,'compare(A,B) -> {A =:= B,A == B,A < B,A =< B,A > B,A >= B}.'),
        ('guard_construct',1,'guard_construct(X) when map_get(a,#{a => X}) =:= X -> X.'),
        ('guard_update',2,'guard_update(M,V) when map_get(a,M#{a := V}) =:= V -> yes; guard_update(_,_) -> no.'),
        ('guard_get',2,'guard_get(K,M) when is_map_key(K,M), map_get(K,M) =:= ok -> yes; guard_get(_,_) -> no.'),
        ('ordered',1,'ordered(M) -> M#{a := hd([])}.'),
        ('independent',1,'independent(X) -> A = #{a=>{X,[X]},b=>1}, B = #{b=>1,a=>{X,[X]}}, A = B, A.'),
        ('wrong_spec',1,'-spec wrong_spec(integer()) -> integer().\nwrong_spec(#{a := V}) -> V; wrong_spec(_) -> no.'),
        ('id',1,'id(X) -> X.')]
    exports = ','.join(f'{name}/{arity}' for name,arity,_ in definitions)
    (work/'answer.erl').write_text(source.split('-module(')[0]+f'-module(answer).\n-export([{exports}]).\n'+
        '\n'.join(body for _,_,body in definitions)+'\n',encoding='utf8')
    (work/'client.erl').write_text('-module(client).\n-export([nested/2,retain/1]).\n'
        'nested(K,V) -> answer:bound_key(K,answer:construct(K,V)).\n'
        'retain(X) -> M = answer:independent(X), answer:literal(), answer:id(M).\n',encoding='utf8')
    maps = [mapping(),mapping(('__struct__','ok')),mapping(('gurka','gaffel')),mapping(('a',1)),mapping(('a','ok'),('b',2)),mapping(('a',2)),
        mapping(('b',2),('a',1)),mapping((1,'int'),(1.0,'float')),
        mapping((0.0,'positive'),(-0.0,'negative')),mapping(((1,),'int')),
        mapping(((1.0,),'float')),mapping(('a',(1,[1,2]))),mapping(('a',1.0)),
        mapping((('tag',2),'ok')),mapping((mapping(('a',1)),'nested'))]
    ordinary = [0,1,1.0,-0.0,0.0,2**100,'a','b','ok',[],(),(1,),(1.0,)]
    values = maps+ordinary
    calls = [('answer',name,[value]) for name,arity,_ in definitions if arity == 1 for value in values]
    calls += [('answer',name,list(pair)) for name in ['compare','repeat','bound_key','get','contains','construct']
              for pair in itertools.product(values,repeat=2)]
    calls += [('answer',name,[value,key,'new']) for name in ['associate','update','mixed','exact_first']
              for value in values for key in ordinary]
    calls += [('answer','check_map_value',[m,k,v]) for m in maps for k in ['a',1,1.0] for v in [1,1.0,'ok']]
    calls += [('answer','map_is_size',[m,n]) for m in values for n in [-1,0,1,2,2.0,'a']]
    calls += [('answer','guard_get',[k,m]) for k in ordinary for m in values]
    calls += [('answer','guard_update',[m,v]) for m in values for v in [1,'ok',mapping()]]
    calls += [('answer','compound_key',[k,m]) for k in [1,1.0,'a'] for m in maps]
    calls += [('answer','key_call',[i,('a','b'),m]) for i in [0,1,2,3,'a'] for m in values]
    calls += [('answer','key_arithmetic',[a,b,m]) for a,b in [(2,2),(1,0),('a',1)] for m in values]
    calls += [('answer','equal_keys',[a,b,m]) for a,b in [('a','a'),('a','b'),(1,1.0)] for m in maps]
    calls += [('answer','empty',[]),('answer','literal',[])]
    calls += [('client','nested',[k,v]) for k in ordinary for v in maps]
    calls += [('client','retain',[v]) for v in values]
    write_calls(work,calls)
    return {'calls':len(calls),'source':str(path.relative_to(otp)),'source_sha256':digest(path),'helpers':helpers,
        'adaptations':'t_update_exact operations use complete parameterized wrappers; t_key_expressions compound/call/division keys use body matches and retain failure; binary/case/fun/message portions remain with their owners; t_duplicate_keys repeated map tests become ordered clauses, supplemented by duplicate/equal key constraints',
        'wrapper_sha256':digest(work/'answer.erl'),'client_sha256':digest(work/'client.erl')}


def main():
    tool,cmake,root,otp_root,directory,settings,config,suffix,escript = sys.argv[1:]
    source, otp, work = pathlib.Path(root), pathlib.Path(otp_root), pathlib.Path(directory)
    work.mkdir(parents=True,exist_ok=True)
    provenance(source, otp, source / "tests/fixtures/patternmatch", work)
    records = kernels(otp, work)
    expected = run([escript, str(source / "tests/compiler/patternmatch/immediate.escript"), str(work)])
    (work / "expected.txt").write_bytes(expected.replace("\r\n", "\n").encode())
    native(tool,cmake,source,work,settings,config,suffix)
    ir = run([tool,'--print-ir',str(work/'answer.erl'),str(work/'client.erl')])
    assert 'map.outcome' in ir and 'map.badmap' in ir and 'map.badkey' in ir
    for bits,triple in [(32,'i686-pc-windows-msvc'),(64,'x86_64-pc-windows-msvc')]:
        run([tool,'--target-triple',triple,'--emit','obj','--artifact-dir',str(work/f'width{bits}'),str(work/'answer.erl'),str(work/'client.erl')])
    (work/'evidence.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
    print(f'{records["calls"]} map OTP/native calls passed in four policies; both word widths emitted.')


if __name__ == '__main__':
    main()

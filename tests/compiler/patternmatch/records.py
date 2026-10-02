"""Execute tuple record expansion with project-owned OTP results and located semantic cases."""
import itertools
import json
import pathlib
import re
import subprocess
import sys
from bindings import compile_case
from evidence import digest, run
from immediate import native
from services import write_calls
from stored import load


def cases(work):
    """Retain literal/default/wildcard lint rules, including accepted dependency gates."""
    rows = []
    for name, body, diagnostic, capability in [
        ('rec_unknown', 'f(X) -> X#missing.a.', 'undefined record', ''),
        ('rec_field', '-record(r,{a}). f(X) -> X#r.missing.', 'undefined record field', ''),
        ('rec_duplicate', '-record(r,{a}). f() -> #r{a=1,a=2}.', 'duplicate record initializer', ''),
        ('rec_decl_duplicate', '-record(r,{a}). -record(r,{b}). f() -> ok.', 'duplicate record declaration', ''),
        ('rec_field_duplicate', '-record(r,{a,a}). f() -> ok.', 'duplicate record field', ''),
        ('rec_forward', 'f() -> #r{}. -record(r,{a}).', 'undefined record', ''),
        ('rec_default_forward', '-record(r,{a=#s{}}). -record(s,{b}). f() -> #r{}.', 'undefined record', ''),
        ('rec_default_self', '-record(r,{a=#r{}}). f() -> #r{}.', 'undefined record', ''),
        ('rec_default_binding', '-record(r,{a=(X=1)}). f() -> #r{}.', 'named variable is illegal', ''),
        ('rec_default_capture', '-record(r,{a=X}). f(X) -> #r{}.', 'named variable is illegal', ''),
        ('rec_default_underscore', '-record(r,{a=(_=1)}). f() -> #r{}.', '', ''),
        ('rec_wild_unused', '-record(r,{a,b}). f(#r{a=1,b=2,_=X}) -> X.', 'record wildcard requires', ''),
        ('rec_wild_twice', '-record(r,{a,b}). f() -> #r{_=1,_=2}.', 'duplicate record wildcard', ''),
        ('rec_wild_name', '-record(r,{a,b}). f() -> #r{X=1}.', "record wildcard field must be '_'", ''),
        ('rec_wild_match', '-record(r,{a,b}). f(#r{_=X}) -> X.', '', ''),
        ('rec_pattern_update', '-record(r,{a}). f(X) -> X#r{a=1} = X.', 'record update is illegal in a pattern', ''),
        ('rec_test_unknown', 'f(X) -> is_record(X,unknown).', 'undefined record', ''),
        ('rec_guard_tag', '-record(r,{a}). f(X) when is_record(X,X) -> ok.', 'record guard tag must', ''),
        ('rec_guard_size', '-record(r,{a}). f(X) when is_record(X,r,X) -> ok.', 'record guard arity must', ''),
        ('rec_guard_size_expr', '-record(r,{a}). f(X) when is_record(X,r,1+2) -> ok.', 'record guard arity must', ''),
        ('rec_guard_update', '-record(r,{a}). f(X) when X#r{a=1} == X -> ok.', 'illegal guard expression', ''),
        ('rec_key_update', '-record(r,{a}). f(X) -> #{(X#r{a=1}) := V} = #{}, V.', 'illegal expression in pattern key or size', ''),
        ('rec_guard_size_negative', '-record(r,{a}). f(X) when is_record(X,r,-1) -> ok.', 'record guard arity must', ''),
        ('rec_index_arithmetic', '-record(r,{a}). f(#r.a+1) -> ok.', 'illegal pattern', ''),
        ('rec_guard_native_tag', 'f(X) when erlang:is_record(X,r,atom) -> ok; f(_) -> no.', '', ''),
        ('rec_guard_bignum', 'f(X) when is_record(X,r,1267650600228229401496703205376) -> ok; f(_) -> no.', '', ''),
        ('rec_body_dynamic', 'f(X) -> is_record(X,X,X).', '', ''),
        ('rec_update_gate', '-record(r,{a}). f(X) -> X#r{a=1}.', '', 'heap expressions'),
        ('rec_fun_default_gate', '-record(r,{a=fun(X)->X end}). f() -> #r{}.', '', 'closures'),
    ]:
        # All authored diagnostic sites remain on line 3, as in the existing CLI nonpublication workflow.
        text = f'-module({name}).\n-compile(nowarn_unused_function).\n{body}\n'
        # Inert warning metadata is unnecessary for the native compiler and is deliberately omitted.
        text = text.replace('-compile(nowarn_unused_function).', '% Record semantic fixture.')
        (work / (name + '.erl')).write_bytes(text.encode())
        rows.append({'name': name, 'body': body, 'diagnostic': diagnostic, 'capability': capability,
                     'sha256': digest(work / (name + '.erl'))})
    terms = [f'{{{r["name"]},{"rejected" if r["diagnostic"] else "accepted"},none}}.' for r in rows]
    (work / 'patterns.term').write_bytes(('\n'.join(terms)+'\n').encode())
    return rows


def kernels(otp, work):
    """Adapt errors/eval_once/nested_access while retaining the specific shape and evaluation obligations."""
    path = otp / 'lib/compiler/test/record_SUITE.erl'
    data = otp / 'lib/compiler/test/record_SUITE_data/record_access_in_guards.erl'
    source = path.read_text(encoding='utf8')
    for witness in ['eval_once(Config)', 'nested_access(Config)', '?assertError({badrecord,Foo}',
                    'true = erlang:is_record(GetRec(), foo)', 'N2#nrec2.nrec1#nrec1.nrec0#nrec0.name']:
        assert witness in source, witness
    header = '''-record(r,{a=default,b,c=3}).
-record(empty,{}).
-record(nrec0,{name = <<"nested0">>}).
-record(nrec1,{name = <<"nested1">>,nrec0 = #nrec0{}}).
-record(nrec2,{name = <<"nested2">>,nrec1 = #nrec1{}}).
-record(d,{a=make_default(),b=make_default()}).
'''
    # Keep the actual include in both the OTP and project compiler inputs.
    (work / 'records.hrl').write_bytes((source.split('-module(')[0] + header).encode())
    definitions = [
        ('default',0,'default() -> #r{}.'),
        ('empty',0,'empty() -> {#empty{},#r.a,#r.b,#r.c}.'),
        ('construct',2,'construct(A,B) -> #r{b=B,a=A}.'),
        ('wild',1,'wild(X) -> #r{a=1,_=X}.'),
        ('wild_alloc',1,'wild_alloc(X) -> #r{_={id(X),[X]}}.'),
        ('ordered',0,'ordered() -> #r{b=(1 div 0),a=((1)#r.a)}.'),
        ('ordered_bind',0,'ordered_bind() -> #r{b=(X=2),a=(X=1)}.'),
        ('default_calls',0,'default_calls() -> #d{}.'),
        ('make_default',0,'make_default() -> {default,<<1:3>>,#{a => 1}}.'),
        ('head',1,'head(#r{b=X}) -> X; head(_) -> no.'),
        ('repeat',1,'repeat(#r{_=X}) -> X; repeat(_) -> no.'),
        ('head_order',1,'head_order(#r{b=X,a=X}) -> X; head_order(_) -> no.'),
        ('alias',1,'alias(#r{a=A,b=B}=R) -> {R,A,B}; alias(_) -> no.'),
        ('body',1,'body(X) -> #r{b=B} = X, B.'),
        ('access',1,'access(X) -> X#r.b.'),
        ('guard_access',1,'guard_access(X) when X#r.a == 1 -> yes; guard_access(_) -> no.'),
        ('guard_alternative',1,'guard_alternative(X) when X#r.a == 1; is_atom(X) -> yes; guard_alternative(_) -> no.'),
        ('guard_construct',1,'guard_construct(X) when (#r{b=X})#r.b =:= X -> yes; guard_construct(_) -> no.'),
        ('test',1,'test(X) -> {is_record(X,r),erlang:is_record(X,empty),is_record(X,r,4),is_record(X,r,native)}.'),
        ('dynamic2',2,'dynamic2(X,T) -> erlang:is_record(X,T).'),
        ('dynamic3',3,'dynamic3(X,T,N) -> erlang:is_record(X,T,N).'),
        ('guard_test',1,'guard_test(X) when is_record(X,r) -> yes; guard_test(_) -> no.'),
        ('legacy',1,'legacy(X) when record(X,r) -> yes; legacy(_) -> no.'),
        ('guard3',1,'guard3(X) when erlang:is_record(X,r,4) -> yes; guard3(_) -> no.'),
        ('guard_zero',1,'guard_zero(X) when is_record(X,r,0) -> yes; guard_zero(_) -> no.'),
        ('guard_big',1,'guard_big(X) when is_record(X,r,10000) -> yes; guard_big(_) -> no.'),
        ('index',1,'index(#r.b) -> yes; index(_) -> no.'),
        ('key',1,'key(M) -> #{#r.b := V} = M, V.'),
        ('eval_once',1,'eval_once(X) -> {is_record(id(X),r),(id(X))#r.a}.'),
        ('nested',1,'nested(N) -> N#nrec2.nrec1#nrec1.nrec0#nrec0.name.'),
        ('nested_defaults',0,'nested_defaults() -> N = #nrec2{}, {N,nested(N)}.'),
        ('retained',1,'retained(X) -> R = #r{b={X,<<1:3>>}}, default_calls(), R.'),
        ('wrong_spec',1,'-spec wrong_spec(integer()) -> integer().\nwrong_spec(X) -> X#r.b.'),
        ('id',1,'id(X) -> X.')]
    exports = ','.join(f'{n}/{a}' for n,a,_ in definitions)
    (work/'answer.erl').write_bytes((source.split('-module(')[0] + f'-module(answer).\n-export([{exports}]).\n-include("records.hrl").\n' + '\n'.join(b for _,_,b in definitions)+'\n').encode())
    (work/'client.erl').write_bytes(b'-module(client).\n-export([retain/1,nested/1]).\nretain(X) -> R = answer:retained(X), answer:default_calls(), answer:id(R).\nnested(X) -> answer:access(answer:id(X)).\n')
    (work/'project.toml').write_bytes(b'schema_version=1\n[[targets]]\nname="records"\nsources=["answer.erl","client.erl"]\n')
    values = [(),('r',),('r',1,2,3),('r',1,1,1),('r',2,2,2),('r',1,2),('other',1,2,3),
              ('r','a',[1,2],{'map':[('a',1)]}),('empty',),('empty',1),0,1.0,'a',[],[1],
              {'map':[(3,42)]},{'bits':'80','length':1},('nrec2','n2',('nrec1','n1',('nrec0','n0')))]
    calls = [('answer',n,[v]) for n,a,_ in definitions if a == 1 for v in values]
    calls += [('answer',n,[]) for n,a,_ in definitions if a == 0]
    calls += [('answer','construct',[a,b]) for a,b in itertools.product(values[:8],repeat=2)]
    calls += [('answer','dynamic2',[v,t]) for v in values for t in ['r','empty','a',1,1.0,(),{'map':[]}]]
    calls += [('answer','dynamic3',[v,t,n]) for v in values for t in ['r',1] for n in [-1,0,1,3,4,5,1.0,'native',2**100,2**59-1]]
    calls += [('client',n,[v]) for n in ['retain','nested'] for v in values]
    write_calls(work,calls)
    return {'calls':len(calls),'cases':cases(work),'sources':{p.relative_to(otp).as_posix():digest(p) for p in [path,data]},
            'adaptations':[
              {'function':'errors/1','kernels':['access/1','body/1','guard_access/1'],'changes':'Expose wrong-tag/arity and badrecord payload through access; update operation remains gated. Remove Common Test catch harness.'},
              {'function':'eval_once/1','kernels':['eval_once/1','default_calls/0','wild_alloc/1'],'changes':'Replace closure/process-dictionary counter with acyclic id/1 calls and IR call counts; preserve evaluation once per accessed operand and once per expanded initializer.'},
              {'function':'nested_access/1','kernels':['nested/1','nested_defaults/0'],'changes':'Retain chained ordinary record accesses and nested defaults; omit the separate update portion, which remains gated.'}],
            'declarations':exports,'wrapper_sha256':digest(work/'answer.erl'),'include_sha256':digest(work/'records.hrl')}


def semantic(tool,work,rows):
    """Exercise source legality, capability separation and failed-batch nonpublication in both CLI modes."""
    (work/'out').mkdir(exist_ok=True)
    (work/'out/sentinel').write_bytes(b'preserve')
    for policy in [['-O0'],['-O0','--no-type-specialization'],['-O2'],['-O2','--no-type-specialization']]:
        for project in [False,True]:
            for row in rows:
                if row['diagnostic'] or row['capability']:
                    compile_case(tool,work,row,policy,project)
    for row in rows:
        if not row['diagnostic'] and not row['capability']:
            run([tool,'--print-types',str(work/(row['name']+'.erl'))])
    assert list((work/'out').iterdir()) == [work/'out/sentinel']


def suites(tool,otp):
    """Parse the original suite and data module with real includes; this is syntax evidence only."""
    options = ['-I',str(otp/'lib/compiler/src'),'--enable-feature','maybe_expr','--disable-feature','compr_assign']
    for app in ['stdlib','kernel','common_test','syntax_tools']:
        options += ['--app-dir',f'{app}={otp/"lib"/app}']
    for name in ['record_SUITE.erl','record_SUITE_data/record_access_in_guards.erl']:
        run([tool,'--preprocess-check',*options,str(otp/'lib/compiler/test'/name)])
        run([tool,'--parse-check',*options,str(otp/'lib/compiler/test'/name)])


def limits(tool,work):
    """Repeated omitted fields and shared nested defaults consume bounded expansion work before publication."""
    fields = ','.join('f'+str(i) for i in range(2000))
    body = '-record(r,{'+fields+'}).\nf() -> '+','.join('#r{}' for _ in range(600))+'.\n'
    path = work/'record_limit.erl'
    path.write_bytes(('-module(record_limit).\n'+body).encode())
    result = subprocess.run([tool,'--emit','obj','--artifact-dir',str(work/'limit-out'),str(path)],capture_output=True,text=True,encoding='utf8',timeout=30)
    assert result.returncode == 1 and 'binding analysis work limit exceeded' in result.stderr,result.stderr
    assert not (work/'limit-out').exists()
    (work/'record_origin.hrl').write_bytes(b'-record(r,{a}).\nf(X) -> X#r.missing.\n')
    (work/'record_origin.erl').write_bytes(b'-module(record_origin).\n-include("record_origin.hrl").\n')
    result = subprocess.run([tool,str(work/'record_origin.erl')],capture_output=True,text=True,encoding='utf8',timeout=30)
    assert result.returncode == 1 and 'record_origin.hrl:2:' in result.stderr and 'undefined record field' in result.stderr,result.stderr


def main():
    tool,cmake,root,directory,settings,config,suffix = sys.argv[1:]
    source,work = pathlib.Path(root),pathlib.Path(directory)
    evidence = load(source,'records',work)
    semantic(tool,work,evidence['cases'])
    limits(tool,work)
    # Semantic project checks use a scratch manifest; restore the reviewed native project afterward.
    stored = source/'tests/fixtures/patternmatch/generated/records/project.toml'
    (work/'project.toml').write_bytes(stored.read_bytes())
    native(tool,cmake,source,work,settings,config,suffix)
    ir = run([tool,'--print-ir',str(work/'answer.erl'),str(work/'client.erl')])
    assert 'record.field' in ir and 'record.test' in ir and 'record.badrecord' in ir
    for name,count in [('eval_once',2),('default_calls',2),('wild_alloc',3)]:
        symbol = 'eav1_616e73776572_' + name.encode().hex() + '_'
        # The generic function contains the source calls exactly once per required evaluation.
        bodies = [b for b in re.findall(r'define .*?\n\}',ir,re.S) if symbol in b.split('{',1)[0]]
        assert len(bodies) == 1,(name,symbol)
        generated = re.findall(r'call i64 @eav1_[0-9a-f_]+\(',bodies[0])
        assert len(generated) == count,(name,generated)
    run([tool,'--print-types',str(work/'answer.erl'),str(work/'client.erl')])
    (work/'evidence.json').write_bytes((json.dumps(evidence,indent=2)+'\n').encode())
    print(f'{evidence["calls"]} record golden calls passed in four policies; {len(evidence["cases"])} semantic cases.')


if __name__ == '__main__':
    main()

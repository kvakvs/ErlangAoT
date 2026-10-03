"""Execute immutable maps, exact keys and scoped key patterns against the pinned OTP oracle."""
import json
import pathlib
import sys
from stored import load
from evidence import run
from immediate import native


def mapping(*entries):
    """Represent exact Erlang keys without Python's integer/float or signed-zero key coalescing."""
    return {'map':list(entries)}


def main():
    tool,cmake,root,directory,settings,config,suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True,exist_ok=True)
    records = load(source, "maps", work)
    native(tool,cmake,source,work,settings,config,suffix)
    ir = run([tool,'--print-ir',str(work/'answer.erl'),str(work/'client.erl')])
    assert 'map.outcome' in ir and 'map.badmap' in ir and 'map.badkey' in ir
    for bits,triple in [(32,'i686-pc-windows-msvc'),(64,'x86_64-pc-windows-msvc')]:
        run([tool,'--target-triple',triple,'--emit','obj','--artifact-dir',str(work/f'width{bits}'),str(work/'answer.erl'),str(work/'client.erl')])
    (work/'evidence.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf8')
    print(f'{records["calls"]} map OTP/native calls passed in four policies; both word widths emitted.')


if __name__ == '__main__':
    main()

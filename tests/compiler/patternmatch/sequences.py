"""Compare body sequences, chained/compound matches and structured failures with OTP."""
import json
import pathlib
import sys
from stored import load
from evidence import run
from immediate import native


def main():
    tool, cmake, root, directory, settings, config, suffix = sys.argv[1:]
    source, work = pathlib.Path(root), pathlib.Path(directory)
    work.mkdir(parents=True, exist_ok=True)
    records = load(source, "sequences", work)
    native(tool, cmake, source, work, settings, config, suffix)
    for mode in ['--print-types', '--print-ir', '--print-optimized-ir']:
        options = [] if mode == '--print-types' else ['-O2']
        run([tool, *options, mode, str(work / 'answer.erl'), str(work / 'client.erl')])
    (work / 'evidence.json').write_text(json.dumps(records, indent=2) + '\n', encoding='utf-8')
    print(f'{records["calls"]} OTP/native sequence and match calls passed in four policies.')


if __name__ == '__main__':
    main()
